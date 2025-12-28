//
// Created by asujy on 2025/12/28.
//

#include "log/Logger.h"

namespace {
    const char* g_levelStr[6] = {"TRACE ", "DEBUG ", "INFO  ",
                                    "WARN  ", "ERROR ", "FATAL "};
}

void Logger::Config(const std::string &file) {
    Stream().FlushAll();
    Stream().SetLogFile(file);
}

std::string Logger::GetFormatedTime() {
    const auto nowTimePoint = std::chrono::system_clock::now();
    const auto nowSeconds =
        std::chrono::system_clock::to_time_t(nowTimePoint);
    const auto nowDuration = nowTimePoint.time_since_epoch();
    const auto nowMilliseconds =
        std::chrono::duration_cast<std::chrono::microseconds>
            (nowDuration % std::chrono::seconds(1)).count();
    std::tm timeInfo;
    localtime_r(&nowSeconds, &timeInfo);

    constexpr std::size_t TIME_BUFFER_SIZE = 32;
    constexpr const char* TIME_FORMAT = "%Y%m%d-%H%M%S";
    constexpr std::size_t RESERVED_CHARS = 2;

    char buffer[TIME_BUFFER_SIZE]{};
    std::size_t nbytes = std::strftime(
        buffer, TIME_BUFFER_SIZE - RESERVED_CHARS,
        TIME_FORMAT,
        &timeInfo);
    buffer[nbytes] = ':';
    buffer[nbytes + 1] = '\0';
    return std::string(buffer) + std::to_string(nowMilliseconds);
}

void Logger::Format(LogLevel level, const char *file, int line) {
    Stream() << '[' << g_levelStr[static_cast<int>(level)];
    Stream() << GetFormatedTime() << ' ';
    if (file) {
        Stream() << file << ':' << line;
    }
    Stream() << "] ";
}


