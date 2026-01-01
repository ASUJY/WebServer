//
// Created by asujy on 2025/12/30.
//

#include "http/HttpConn.h"
#include "log/Logger.h"
#include "common-lib/Utils.h"

#include <sys/epoll.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstdarg>

std::atomic<int> HttpConn::m_epollfd{-1};
std::atomic<int> HttpConn::m_user_count{0};

void HttpConn::Init(int sockfd, const sockaddr_in &addr) {
    m_sockfd = sockfd;
    m_addr = addr;

    // 设置端口复用
    int reuse = 1;
    int ret = setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (ret == -1) {
        LOG_ERROR << "setsockopt failed!!!";
        return;
    }
    AddFD(m_epollfd.load(), m_sockfd, true);
    m_user_count += 1;
    init();
}

void HttpConn::init() {
    m_url = nullptr;
    m_version = nullptr;
    m_checkState = http::CHECK_STATE::CHECK_STATE_REQUESTLINE;
    m_method = http::HTTP_METHOD::GET;
    m_readIndex = 0;
    m_writeIndex = 0;
    m_checkedIndex = 0;
    m_startLine = 0;
    m_readBuffer.fill('\0');
    m_writeBuffer.fill('\0');
    m_linger = false;
    m_contentLength = 0;
    m_host.clear();
    m_realFile.clear();
    m_bytesToSend = 0;
    m_bytesHaveSend = 0;
}

void HttpConn::CloseConn() {
    if (m_sockfd != -1) {
        DelFD(m_epollfd.load(), m_sockfd);
        m_sockfd = -1;
        m_user_count -= 1;
    }
}

bool HttpConn::Read() {
    if (m_readIndex >= READ_BUFFER_SIZE) {
        return false;
    }
    ssize_t bytesRead{0};
    while (true) {
        bytesRead = ::recv(m_sockfd, m_readBuffer.data() + m_readIndex,
            READ_BUFFER_SIZE - m_readIndex, 0);
        if (bytesRead == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下无数据可读
                break;
            }
            return false;
        } else if (bytesRead == 0) {
            // 对方关闭连接
            return false;
        }
        m_readIndex += static_cast<std::size_t>(bytesRead);
    }

    if (m_readIndex < READ_BUFFER_SIZE) {
        m_readBuffer[m_readIndex] = '\0';
    } else {
        m_readBuffer[READ_BUFFER_SIZE - 1] = '\0';
    }

    LOG_INFO << "读取到了数据: " << m_readBuffer.data();
    return true;
}

/*
 * 解析一行，判断依据 \r\n
 */
http::LINE_STATUS HttpConn::ParseLine() {
    char temp = 0;
    for (; m_checkedIndex < m_readIndex; ++m_checkedIndex) {
        temp = m_readBuffer[m_checkedIndex];
        if (temp == '\r') {
            if (m_checkedIndex + 1 == m_readIndex) {
                return http::LINE_STATUS::LINE_OPEN;
            } else if (m_readBuffer[m_checkedIndex + 1] == '\n') {
                m_readBuffer[m_checkedIndex++] = '\0';
                m_readBuffer[m_checkedIndex++] = '\0';
                return http::LINE_STATUS::LINE_OK;
            }
            return http::LINE_STATUS::LINE_BAD;
        } else if (temp == '\n') {
            if (m_checkedIndex > 1 && m_readBuffer[m_checkedIndex - 1] == '\r') {
                m_readBuffer[m_checkedIndex - 1] = '\0';
                m_readBuffer[m_checkedIndex++] = '\0';
                return http::LINE_STATUS::LINE_OK;
            }
            return http::LINE_STATUS::LINE_BAD;
        }
    }
    return http::LINE_STATUS::LINE_OPEN;
}

/*
 * GET /index.html HTTP/1.1
 * m_url = /index.html
 * method = GET
 * m_version = HTTP/1.1
 */
http::HTTP_CODE HttpConn::ParseRequestLine(char *text) {
    m_url = std::strpbrk(text, " \t");
    if (!m_url) {
        return http::HTTP_CODE::BAD_REQUEST;
    }
    *m_url++ = '\0';

    /* 目前仅支持GET */
    char* method = text;
    if (strcasecmp(method, "GET") == 0) {
        m_method = http::HTTP_METHOD::GET;
    } else {
        return http::HTTP_CODE::BAD_REQUEST;
    }

    /* 目前仅支持 HTTP/1.1 */
    m_version = std::strpbrk(m_url, " \t");
    if (!m_version) {
        return http::HTTP_CODE::BAD_REQUEST;
    }
    *m_version++ = '\0';
    if (strcasecmp(m_version, "HTTP/1.1") != 0 ) {
        return http::HTTP_CODE::BAD_REQUEST;
    }

    if (strncasecmp(m_url, "http://", strlen("http://")) == 0) {
        m_url += strlen("http://");         //  192.168.192.1:10000/index.html
        m_url = std::strchr(m_url, '/');    // /index.html
    }
    if (!m_url || m_url[0] != '/') {
        return http::HTTP_CODE::BAD_REQUEST;
    }

    m_checkState = http::CHECK_STATE::CHECK_STATE_HEADER;
    return http::HTTP_CODE::NO_REQUEST;
}

http::HTTP_CODE HttpConn::ParseHeaders(char *text) {
    std::string headerText(text);
    if(headerText.empty()) {
        if (m_contentLength != 0) {
            m_checkState = http::CHECK_STATE::CHECK_STATE_CONTENT;
            return http::HTTP_CODE::NO_REQUEST;
        }
        return http::HTTP_CODE::GET_REQUEST;
    }

    std::string lowerText = headerText;
    for (auto& ch : lowerText) {
        ch = std::tolower(static_cast<unsigned char>(ch));
    }
    if (lowerText.find("connection:") == 0) {
        auto colonPos  = lowerText.find(":");
        if (colonPos != std::string::npos) {
            std::string value = headerText.substr(colonPos);
            auto firstNonWs = value.find_first_not_of(" \t");
            if (firstNonWs != std::string::npos) {
                value.erase(0, firstNonWs + 1 + 1 );
                if (value.find("keep-alive") == 0) {
                    m_linger = true;
                    LOG_INFO << "connection: keep-alive";
                }
            }
        }
    } else if (lowerText.find("content-length") == 0) {
        auto colonPos = lowerText.find(":");
        if (colonPos != std::string::npos) {
            std::string value = headerText.substr(colonPos + 1);
            auto firstNonWs = value.find_first_not_of(" \t");
            if (firstNonWs != std::string::npos) {
                value = value.substr(firstNonWs);
                m_contentLength = std::stoi(value);
                LOG_INFO << "content-length: " << m_contentLength;
            }
        }
    } else if (lowerText.find("host") == 0) {
        // 处理Host头部字段
        auto colonPos = lowerText.find(":");
        if (colonPos != std::string::npos) {
            std::string value = headerText.substr(colonPos + 1);
            auto firstNonWs = value.find_first_not_of(" \t");
            if (firstNonWs != std::string::npos) {
                m_host = value.substr(firstNonWs);
                LOG_INFO << "host: " << m_host;
            }
        }
    } else {
        LOG_ERROR << "oop! unknow header: " << lowerText;
    }
    return http::HTTP_CODE::NO_REQUEST;
}

/*
 * 没有真正解析HTTP请求的消息体，只是判断它是否被完整的读入了
 */
http::HTTP_CODE HttpConn::ParseContent(char* text) {
    if (text == nullptr) {
        return http::HTTP_CODE::BAD_REQUEST;
    }
    if (m_readIndex >= (m_contentLength + m_checkedIndex)) {
        text[m_contentLength] = '\0';
        LOG_INFO << "请求体：" << text;
        return http::HTTP_CODE::GET_REQUEST;
    }
    return http::HTTP_CODE::NO_REQUEST;
}

http::HTTP_CODE HttpConn::DoRequest() {
    std::string fullPath = GetExecutableDir();
    if (fullPath.empty()) {
        LOG_ERROR << "Can not get executable path!!!";
        return http::HTTP_CODE::BAD_REQUEST;
    }
    fullPath += "/../resources";
    fullPath += m_url;
    m_realFile = fullPath;
    LOG_DEBUG << "fullPath: " << fullPath;
    if (stat(m_realFile.c_str(), &m_fileStat) < 0) {
        LOG_WARN << "No Resource";
        return http::HTTP_CODE::NO_RESOURCE;
    }

    if (!m_fileStat.st_mode & S_IROTH) {
        LOG_WARN << "Client has not permission!";
        return http::HTTP_CODE::FORBIDDEN_REQUEST;
    }

    if (S_ISDIR(m_fileStat.st_mode)) {
        return http::HTTP_CODE::BAD_REQUEST;
    }

    const int fd = open(m_realFile.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return http::HTTP_CODE::NO_RESOURCE;
    }
    // 把资源文件映射到内存中
    m_fileAddress = reinterpret_cast<char*>(mmap(0, m_fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (m_fileAddress == MAP_FAILED) {
        close(fd);
        LOG_ERROR << "mmap failed!";
        return http::HTTP_CODE::INTERNAL_ERROR;
    }
    close(fd);
    return http::HTTP_CODE::FILE_REQUEST;
}

http::HTTP_CODE HttpConn::ProcessRead() {
    http::LINE_STATUS lineStatus{http::LINE_STATUS::LINE_OK};
    http::HTTP_CODE ret{http::HTTP_CODE::NO_REQUEST};
    char* text{nullptr};

    while (
        ((m_checkState == http::CHECK_STATE::CHECK_STATE_CONTENT) &&
        (lineStatus == http::LINE_STATUS::LINE_OK))
        || ((lineStatus = ParseLine()) == http::LINE_STATUS::LINE_OK)) {
        text = GetLine();
        m_startLine = m_checkedIndex;
        LOG_INFO << "got 1 http line: " << text;

        switch (m_checkState) {
            case http::CHECK_STATE::CHECK_STATE_REQUESTLINE: {
                ret = ParseRequestLine(text);
                if (ret == http::HTTP_CODE::BAD_REQUEST) {
                    return http::HTTP_CODE::BAD_REQUEST;
                }
                break;
            }
            case http::CHECK_STATE::CHECK_STATE_HEADER: {
                ret = ParseHeaders(text);
                if (ret == http::HTTP_CODE::BAD_REQUEST) {
                    return http::HTTP_CODE::BAD_REQUEST;
                } else if (ret == http::HTTP_CODE::GET_REQUEST) {
                    return DoRequest();
                }
                break;
            }
            case http::CHECK_STATE::CHECK_STATE_CONTENT: {
                ret = ParseContent(text);
                if (ret == http::HTTP_CODE::GET_REQUEST) {
                    return DoRequest();
                }
                lineStatus = http::LINE_STATUS::LINE_OPEN;
                break;
            }
            default: {
                return http::HTTP_CODE::INTERNAL_ERROR;
            }
        }
        ret = http::HTTP_CODE::NO_REQUEST;
    }
    return ret;
}

void HttpConn::Unmap() {
    if (m_fileAddress) {
        munmap(m_fileAddress, m_fileStat.st_size);
        m_fileAddress = nullptr;
    }
}


bool HttpConn::Write() {
    int temp = 0;

    // 待发送字节数为0，响应结束
    if (m_bytesToSend == 0) {
        ModFD(m_epollfd.load(), m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (true) {
        temp = writev(m_sockfd, m_iv, m_ivCount);
        if (temp <= -1) {
            if (errno == EAGAIN) {
                ModFD(m_epollfd.load(), m_sockfd, EPOLLOUT);
                return true;
            }
            Unmap();
            return false;
        }

        m_bytesHaveSend += temp;
        m_bytesToSend -= temp;
        if (m_bytesHaveSend >= m_iv[0].iov_len) {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_fileAddress + (m_bytesHaveSend - m_writeIndex);
            m_iv[1].iov_len = m_bytesToSend;
        } else {
            m_iv[0].iov_base = m_iv[0].iov_base + temp;
            m_iv[0].iov_len -= temp;
        }

        // 所有数据发送完毕
        if (m_bytesToSend <= 0) {
            Unmap();
            ModFD(m_epollfd.load(), m_sockfd, EPOLLIN);

            if (m_linger) {
                init();
                return true;
            } else {
                return false;
            }
        }
    }
}

bool HttpConn::AddResponse(const char * format, ...) {
    if (m_writeIndex >= WRITE_BUFFER_SIZE) {
        return false;
    }

    va_list args;
    va_start(args, format);
    const int remainSize = WRITE_BUFFER_SIZE - 1 - m_writeIndex;
    auto len = vsnprintf(&m_writeBuffer[m_writeIndex], remainSize, format, args);
    if (len >= remainSize) {
        va_end(args); // 提前释放参数列表，避免资源泄漏
        return false;
    }
    m_writeIndex += len;
    va_end(args);
    return true;
}

bool HttpConn::AddStatusLine(int status, const char *title) {
    return AddResponse("HTTP/1.1 %d %s\r\n", status, title);
}

bool HttpConn::AddContentLength(int contentLength) {
    return AddResponse("Content-Length: %d\r\n", contentLength);
}

bool HttpConn::AddContentType() {
    return AddResponse("Content-Type:%s\r\n", "text/html");
}

bool HttpConn::AddLinger() {
    return AddResponse("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool HttpConn::AddBlankLine() {
    return AddResponse("%s", "\r\n");
}

bool HttpConn::AddHeader(int contentLength) {
    AddContentLength(contentLength);
    AddContentType();
    AddLinger();
    AddBlankLine();
}

bool HttpConn::AddContent(const char *content) {
    return AddResponse("%s", content);
}

bool HttpConn::ProcessWrite(http::HTTP_CODE ret) {
    switch (ret) {
        case http::HTTP_CODE::INTERNAL_ERROR:
            AddStatusLine(500, http::status::ERROR_500_TITLE);
            AddHeader(std::strlen(http::status::ERROR_500_FORM));
            if (!AddContent(http::status::ERROR_500_FORM)) {
                LOG_ERROR << "Add Content failed!!!";
                return false;
            }
            break;
        case http::HTTP_CODE::BAD_REQUEST:
            AddStatusLine(400, http::status::ERROR_400_TITLE);
            AddHeader(std::strlen(http::status::ERROR_400_FORM));
            if (!AddContent(http::status::ERROR_400_FORM)) {
                LOG_ERROR << "Add Content failed!!!";
                return false;
            }
            break;
        case http::HTTP_CODE::NO_RESOURCE:
            AddStatusLine(404, http::status::ERROR_404_TITLE);
            AddHeader(strlen(http::status::ERROR_404_FORM));
            if (!AddContent(http::status::ERROR_404_FORM)) {
                LOG_ERROR << "Add Content failed!!!";
                return false;
            }
            break;
        case http::HTTP_CODE::FORBIDDEN_REQUEST:
            AddStatusLine(403, http::status::ERROR_403_TITLE);
            AddHeader(strlen(http::status::ERROR_403_FORM));
            if (!AddContent(http::status::ERROR_403_FORM)) {
                LOG_ERROR << "Add Content failed!!!";
                return false;
            }
            break;
        case http::HTTP_CODE::FILE_REQUEST:
            AddStatusLine(200, http::status::OK_200_TITLE);
            AddHeader(m_fileStat.st_size);
            m_iv[0].iov_base = m_writeBuffer.data();
            m_iv[0].iov_len = m_writeIndex;
            m_iv[1].iov_base = m_fileAddress;
            m_iv[1].iov_len = m_fileStat.st_size;
            m_ivCount = 2;
            m_bytesToSend = m_writeIndex + m_fileStat.st_size;
            return true;
        default:
            return false;
    }
    m_iv[0].iov_base = m_writeBuffer.data();
    m_iv[0].iov_len = m_writeIndex;
    m_ivCount = 1;
    m_bytesToSend = m_writeIndex;
    return true;
}


void HttpConn::Process() {
    http::HTTP_CODE readRet = ProcessRead();
    if (readRet == http::HTTP_CODE::NO_REQUEST) {
        ModFD(m_epollfd.load(), m_sockfd, EPOLLIN);
        return;
    }

    bool writeRet = ProcessWrite(readRet);
    if (!writeRet) {
        CloseConn();
    }
    ModFD(m_epollfd.load(), m_sockfd, EPOLLOUT);
}
