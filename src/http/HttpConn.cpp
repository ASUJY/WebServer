//
// Created by asujy on 2025/12/30.
//

#include "http/HttpConn.h"
#include "log/Logger.h"
#include "common-lib/Utils.h"

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
    AddFD(m_epollfd, m_sockfd, true);
    m_user_count += 1;
}

void HttpConn::CloseConn() {
    if (m_sockfd != -1) {
        DelFD(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count -= 1;
    }
}

bool HttpConn::Read() {
    LOG_DEBUG << "一次性读完数据";
    return true;
}

bool HttpConn::Write() {
    LOG_DEBUG << "一次性写完数据";
    return true;
}
