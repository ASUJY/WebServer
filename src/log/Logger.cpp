//
// Created by asujy on 2025/12/28.
//

#include "log/Logger.h"

#include <iomanip>
#include <sstream>

int Logger::m_today{0};
unsigned long long Logger::m_lineCount{0};

namespace {
    const char* g_levelStr[6] = {"TRACE ", "DEBUG ", "INFO  ",
                                    "WARN  ", "ERROR ", "FATAL "};
}

void Logger::Config(const std::string &file) {
    Stream().FlushAll();
    Stream().SetLogFileBasename(file);
    Stream().SetLogFile(file);
    std::tm timeInfo{};
    const auto nowTimePoint = std::chrono::system_clock::now();
    const auto nowSeconds =
        std::chrono::system_clock::to_time_t(nowTimePoint);
    localtime_r(&nowSeconds, &timeInfo);
    m_today = timeInfo.tm_mday;
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
    /* 检查是否需要分割日志文件（日期变化或行数超限） */
    NeedRoll();

    Stream() << '[' << g_levelStr[static_cast<int>(level)];
    Stream() << GetFormatedTime() << ' ';
    if (file) {
        Stream() << file << ':' << line;
    }
    Stream() << "] ";

    {
        std::lock_guard<std::mutex> locker(m_mtx);
        m_lineCount++;
    }
}

void Logger::NeedRoll() {
    std::unique_lock<std::mutex> locker(m_mtx);
    bool sizeRoll = (m_lineCount && (m_lineCount % MAX_LINES == 0));
    locker.unlock();
    std::tm timeInfo{};
    const auto nowTimePoint = std::chrono::system_clock::now();
    const auto nowSeconds =
        std::chrono::system_clock::to_time_t(nowTimePoint);
    localtime_r(&nowSeconds, &timeInfo);
    bool timeRoll = (m_today != timeInfo.tm_mday);

    std::string filename = GenerateFilename();
    locker.lock();
    if (sizeRoll) {
        std::ostringstream filenameOss;
        filenameOss << filename << "-" << (m_lineCount / MAX_LINES);
        SetLogFile(filenameOss.str());
    } else if (timeRoll) {
        SetLogFile(filename);
        m_today = timeInfo.tm_mday;
        m_lineCount = 0;
    }
    locker.unlock();
}

std::string Logger::GenerateFilename() {
    std::tm timeInfo{};
    const auto nowTimePoint = std::chrono::system_clock::now();
    const auto nowSeconds =
        std::chrono::system_clock::to_time_t(nowTimePoint);
    localtime_r(&nowSeconds, &timeInfo);
    std::string logFile = GetLogFileBasename();
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(logFile.size()) << logFile << "."
        << std::setw(4) << (timeInfo.tm_year + 1900) << "_"
        << std::setw(2) << (timeInfo.tm_mon + 1) << "_"
        << std::setw(2) << timeInfo.tm_mday;
    std::string filename = oss.str();
    return filename;
}