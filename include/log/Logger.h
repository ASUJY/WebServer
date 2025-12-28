//
// Created by asujy on 2025/12/28.
//

#ifndef LOGGER_H
#define LOGGER_H

#include "log/LogStream.h"

class Logger {
public:
    enum class LogLevel : int {
        TRACE = 0, DEBUG, INFO, WARN, ERROR, FATAL
    };

    Logger(LogLevel level, const char* file, int line) {
        Format(level, file, line);
    }

    ~Logger() {
        Stream() << "\n";
        Stream().FlushLine();
    }

    // 单例模式
    static LogStream& Stream() {
        static LogStream stream("/dev/stdout");
        return stream;
    }

    // 配置日志文件
    static void Config(const std::string &file);

private:
    void Format(LogLevel level, const char* file, int line);
    std::string GetFormatedTime();
};

#ifndef LOG_LEVEL
#define LOG_LEVEL (Logger::LogLevel::DEBUG)
#endif

#define LOG_TRACE \
if (Logger::LogLevel::TRACE >= LOG_LEVEL) \
    Logger(Logger::LogLevel::TRACE, __FILE__, __LINE__).Stream()
#define LOG_DEBUG \
if (Logger::LogLevel::DEBUG >= LOG_LEVEL) \
    Logger(Logger::LogLevel::DEBUG, __FILE__, __LINE__).Stream()
#define LOG_INFO                   \
if (Logger::LogLevel::INFO >= LOG_LEVEL) \
    Logger(Logger::LogLevel::INFO, nullptr, 0).Stream()
#define LOG_WARN                   \
if (Logger::LogLevel::WARN >= LOG_LEVEL) \
    Logger(Logger::LogLevel::WARN, __FILE__, __LINE__).Stream()
#define LOG_ERROR                   \
if (Logger::LogLevel::ERROR >= LOG_LEVEL) \
    Logger(Logger::LogLevel::ERROR, __FILE__, __LINE__).Stream()
#define LOG_FATAL                   \
if (Logger::LogLevel::FATAL >= LOG_LEVEL) \
    Logger(Logger::LogLevel::FATAL, __FILE__, __LINE__).Stream()
#endif //LOGGER_H
