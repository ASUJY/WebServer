//
// Created by asujy on 2025/12/30.
//

#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <atomic>
#include <arpa/inet.h>
#include <array>
#include <sys/stat.h>

namespace http {
    namespace status {
        constexpr const char* OK_200_TITLE = "OK";
        constexpr const char* ERROR_400_TITLE = "Bad Request";
        constexpr const char* ERROR_400_FORM = "Your request has bad syntax or is inherently impossible to satisfy.";
        constexpr const char* ERROR_403_TITLE = "Forbidden";
        constexpr const char* ERROR_403_FORM = "You do not have permission to get file from this server.";
        constexpr const char* ERROR_404_TITLE = "Not Found";
        constexpr const char* ERROR_404_FORM = "The requested file was not found on this server.";
        constexpr const char* ERROR_500_TITLE = "Internal Error";
        constexpr const char* ERROR_500_FORM = "There was an unusual problem serving the requested file.";
    }

    enum class HTTP_METHOD : int {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT
    };

    enum class CHECK_STATE : int {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };

    enum class LINE_STATUS : int {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

    enum class HTTP_CODE : int {
        NO_REQUEST = 0,      // 请求不完整，需要继续读取客户数据
        GET_REQUEST,         // 获得一个完整的客户端请求
        BAD_REQUEST,         // 客户端的请求有语法错误
        NO_RESOURCE,         // 服务器无资源
        FORBIDDEN_REQUEST,   // 客户端对资源没有足够的访问权限
        FILE_REQUEST,        // 文件请求成功
        INTERNAL_ERROR,      // 服务器内部错误
        CLOSED_CONNECTION    // 客户端关闭连接
    };
}

class HttpConn {
public:
    static constexpr uint32_t READ_BUFFER_SIZE = 4096;
    static constexpr uint32_t WRITE_BUFFER_SIZE = 2048;

    HttpConn() = default;
    virtual ~HttpConn() = default;

    // 禁止拷贝构造和拷贝赋值
    HttpConn(const HttpConn&) = delete;
    HttpConn& operator=(const HttpConn&) = delete;
    HttpConn(HttpConn &&) noexcept = default;
    HttpConn& operator=(HttpConn &&) noexcept = default;

    void Init(int sockfd, const sockaddr_in &addr);
    void CloseConn();

    bool Read();
    bool Write();

    static int GetUserCount() {
        return m_user_count.load();
    }

    static void SetEpollFD(int fd) {
        m_epollfd.store(fd);
    }

    void Process();

private:
    void init();
    http::HTTP_CODE ProcessRead();
    bool ProcessWrite(http::HTTP_CODE ret);

    /* ProcessRead() use these functions */
    http::HTTP_CODE ParseRequestLine(char* text);
    http::HTTP_CODE ParseHeaders(char* text);
    http::HTTP_CODE ParseContent(char* text);
    http::LINE_STATUS ParseLine();
    char* GetLine() {
        return m_readBuffer.data() + m_startLine;
    }
    http::HTTP_CODE DoRequest();

    /* ProcessWrite() use these functions */
    bool AddResponse(const char* format, ...);
    bool AddStatusLine(int status, const char* title);
    bool AddHeader(int contentLength);
    bool AddContentLength(int contentLength);
    bool AddContentType();
    bool AddLinger();
    bool AddBlankLine();
    bool AddContent(const char* content);
    void Unmap();  // 对内存映射区执行munmap操作

private:
    int m_sockfd = -1;
    sockaddr_in m_addr{};

    std::size_t m_readIndex{0};
    std::array<char, READ_BUFFER_SIZE> m_readBuffer;
    std::size_t m_checkedIndex{0};
    std::size_t m_startLine{0};

    char* m_url{nullptr};
    char* m_version{nullptr};
    std::string m_host;
    http::CHECK_STATE m_checkState{http::CHECK_STATE::CHECK_STATE_REQUESTLINE};
    http::HTTP_METHOD m_method{http::HTTP_METHOD::GET};
    int m_contentLength{0};
    bool m_linger{false};
    std::string m_realFile;

    std::size_t m_writeIndex = 0;
    std::array<char, WRITE_BUFFER_SIZE> m_writeBuffer;
    struct stat m_fileStat{};
    char* m_fileAddress{nullptr};  // 资源文件
    struct iovec m_iv[2];
    int m_ivCount{0};
    int m_bytesToSend{0};
    int m_bytesHaveSend{0};

    static std::atomic<int> m_epollfd;
    static std::atomic<int> m_user_count;
};

#endif //HTTPCONN_H
