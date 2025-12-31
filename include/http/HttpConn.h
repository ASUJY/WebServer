//
// Created by asujy on 2025/12/30.
//

#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <atomic>
#include <arpa/inet.h>
#include <array>

namespace http {
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

    /* ProcessRead() use these functions */
    http::HTTP_CODE ParseRequestLine(char* text);
    http::HTTP_CODE ParseHeaders(char* text);
    http::HTTP_CODE ParseContent(char* text);
    http::LINE_STATUS ParseLine();
    char* GetLine() {
        return m_readBuffer.data() + m_startLine;
    }
    http::HTTP_CODE DoRequest();

private:
    int m_sockfd = -1;
    sockaddr_in m_addr{};

    std::size_t m_readIndex{0};
    std::array<char, READ_BUFFER_SIZE> m_readBuffer;
    std::size_t m_checkedIndex{0};
    std::size_t m_startLine{0};

    char* m_url{nullptr};
    char* m_version{nullptr};
    http::CHECK_STATE m_checkState{http::CHECK_STATE::CHECK_STATE_REQUESTLINE};
    http::HTTP_METHOD m_method{http::HTTP_METHOD::GET};

    static std::atomic<int> m_epollfd;
    static std::atomic<int> m_user_count;
};

#endif //HTTPCONN_H
