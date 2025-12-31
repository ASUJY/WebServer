//
// Created by asujy on 2025/12/30.
//

#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <atomic>
#include <arpa/inet.h>

class HttpConn {
public:
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
    int m_sockfd = -1;
    sockaddr_in m_addr{};

    static std::atomic<int> m_epollfd;
    static std::atomic<int> m_user_count;
};

#endif //HTTPCONN_H
