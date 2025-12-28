//
// Created by asujy on 2025/12/28.
//

#include "log/Logger.h"

int main() {
    Logger::Config("Web.log");
    LOG_TRACE << "Trace log";
    LOG_DEBUG << "Debug log";
    LOG_INFO << "Info log";
    LOG_WARN << "Warn log";
    LOG_ERROR << "Error log";
    LOG_FATAL << "Fatal log";
    return 0;
}