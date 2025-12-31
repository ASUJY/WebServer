//
// Created by asujy on 2025/12/28.
//

#include <iostream>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <memory>

#include "log/Logger.h"
#include "common-lib/Utils.h"
#include "http/HttpConn.h"
#include "common-lib/ThreadPool.h"

constexpr int LISTEN_BACKLOG = 8;
constexpr int MAX_EVENT_NUMBER = 10000; // 监听的最大的事件数量
constexpr int EPOLL_INSTANCE_SIZE = 100; // useless
constexpr int MAX_FD = 65535;

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        std::string filename = "programe";
        if (argc > 0 && argv[0]) {
            filename = argv[0];
            filename = GetBasename(filename);
        }
        std::cout << "Usage: " << filename << " port_number!" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    Logger::Config("Web.log");
    int port = std::atoi(argv[1]);
    LOG_INFO << "WebServer port: " << port;

    AddSignal(SIGPIPE, SIG_IGN);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        LOG_ERROR << "socket failed!!!";
        std::exit(EXIT_FAILURE);
    }

    // 设置端口复用
    int reuse{1};
    int ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (ret == -1) {
        LOG_ERROR << "setsockopt failed!!!";
        std::exit(EXIT_FAILURE);
    }

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    ret = bind(listenfd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
    if (ret == -1) {
        LOG_ERROR << "bind failed";
        std::exit(EXIT_FAILURE);
    }

    ret = listen(listenfd, LISTEN_BACKLOG);
    if (ret == -1) {
        LOG_ERROR << "listen failed";
        std::exit(EXIT_FAILURE);
    }

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(EPOLL_INSTANCE_SIZE);
    AddFD(epollfd, listenfd, false);
    HttpConn::SetEpollFD(epollfd);

    std::unique_ptr<ThreadPool<HttpConn>> pool(new ThreadPool<HttpConn>);
    std::unique_ptr<HttpConn[]> users(new HttpConn[MAX_FD]);
    while (true) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            LOG_ERROR << "epoll_wait failed";
            break;
        }

        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                struct sockaddr_in clientAddress{};
                socklen_t clientAddressLength = sizeof(clientAddress);
                int connfd = accept(listenfd,
                    reinterpret_cast<struct sockaddr*>(&clientAddress),
                    &clientAddressLength);
                if (connfd == -1) {
                    LOG_ERROR << "accept failed!!!";
                    continue;
                }
                if (HttpConn::GetUserCount() >= MAX_FD) {
                    close(connfd);
                    continue;
                }
                users[connfd].Init(connfd, clientAddress);
                LOG_INFO<< "Client Address: "
                    << inet_ntoa(clientAddress.sin_addr);
                LOG_INFO << "Client Port: " << ntohs(clientAddress.sin_port);
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP |EPOLLERR)) {
                users[sockfd].CloseConn();
            } else if (events[i].events & EPOLLIN) {
                if (users[sockfd].Read()) {
                    pool->Append(&users[sockfd]);
                } else {
                    users[sockfd].CloseConn();
                }
            } else if (events[i].events & EPOLLOUT) {
                if (!users[sockfd].Write()) {
                    users[sockfd].CloseConn();
                }
            }
        }
    }
    close(epollfd);
    close(listenfd);
    return 0;
}