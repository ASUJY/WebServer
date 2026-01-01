//
// Created by asujy on 2025/12/29.
//

#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <signal.h>

using SignalHandler = void(*)(int);

void AddSignal(int sig, SignalHandler handler);

std::string GetBasename(const std::string& path);
std::string GetExecutableDir();

void AddFD(int epollfd, int fd, bool oneShot);
void DelFD(int epollfd, int fd);
void ModFD(int epollfd, int fd, int ev);

#endif //UTILS_H
