//
// Created by asujy on 2025/12/29.
//

#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#include "common-lib/Utils.h"
#include "log/Logger.h"

std::string GetBasename(const std::string &path) {
    std::string basename = path;
    auto lastSlash = basename.find_last_of("/");
    if (lastSlash != std::string::npos) {
        basename = basename.substr(lastSlash + 1);
    } else {
        basename = "";
    }
    return basename;
}

void AddSignal(int sig, SignalHandler handler) {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sigfillset(&sa.sa_mask) == -1) {
        LOG_ERROR << "sigfillset failed!!!";
        std::exit(EXIT_FAILURE);
    }
    if (sigaction(sig, &sa, NULL) == -1) {
        LOG_ERROR << "sigaction failed!!!";
        std::exit(EXIT_FAILURE);
    }
}

// 设置文件描述符为非阻塞模式
static int SetNonBlocking(int fd) {
    int oldOption{-1};
    if ((oldOption = fcntl(fd, F_GETFL)) == -1) {
        LOG_ERROR << "Failed to get file descriptor flags (F_GETFL): "
            << std::strerror(errno);
        return -1;
    }

    int newOption{oldOption | O_NONBLOCK};

    if (fcntl(fd, F_SETFL, newOption) == -1) {
        LOG_ERROR << "Failed to get file descriptor flags (F_GETFL): "
            << std::strerror(errno);
        return -1;
    }
    return oldOption;
}

void AddFD(int epollfd, int fd, bool oneShot) {
    struct epoll_event event{};
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;
    if (oneShot) {
        event.events |= EPOLLONESHOT;
    }
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) == -1) {
        LOG_ERROR << "Failed to add fd to epoll (EPOLL_CTL_ADD): "
            << strerror(errno);
        return;
    }
    SetNonBlocking(fd);
}

void DelFD(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}
