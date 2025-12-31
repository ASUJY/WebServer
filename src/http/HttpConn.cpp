//
// Created by asujy on 2025/12/30.
//

#include "http/HttpConn.h"
#include "log/Logger.h"
#include "common-lib/Utils.h"

#include <sys/epoll.h>

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
    m_checkedIndex = 0;
    m_startLine = 0;
    m_readBuffer.fill('\0');
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

bool HttpConn::Write() {
    LOG_DEBUG << "一次性写完数据";
    return true;
}

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

http::HTTP_CODE HttpConn::ParseRequestLine(char *text) {
    m_url = std::strpbrk(text, " \t");
    if (m_url) {
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

}


http::HTTP_CODE HttpConn::ParseContent(char* text) {

}

http::HTTP_CODE HttpConn::DoRequest() {

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


void HttpConn::Process() {
    http::HTTP_CODE readRet = ProcessRead();
    if (readRet == http::HTTP_CODE::NO_REQUEST) {
        ModFD(m_epollfd.load(), m_sockfd, EPOLLIN);
        return;
    }
    LOG_INFO << "parse request";
}
