//
// Created by asujy on 2025/12/28.
//

#ifndef LOGSTREAM_H
#define LOGSTREAM_H

#include <mutex>
#include <fstream>
#include <cstring>
#include "LogStreamBuf.h"

#ifndef LOG_LINE_BUFFER_SIZE
#define LOG_LINE_BUFFER_SIZE (1 << 12)  // 4KB行缓冲区
#endif

#ifndef LOG_BUFFER_SIZE
#define LOG_BUFFER_SIZE (1 << 13)       // 8KB总缓冲区
#endif

class LogStream {
public:
    explicit LogStream(const std::string &logFile) :
        m_logFile(logFile), m_logFileBasename(logFile),
        m_lineBuf(LOG_LINE_BUFFER_SIZE), m_buf(LOG_BUFFER_SIZE) {}
    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

    ~LogStream() {
        FlushAll();
        Close();
    }

    void SetLogFile(const std::string &file) {
        std::lock_guard<std::mutex> locker(m_mtx);
        m_logFile = file;
    }

    void SetLogFileBasename(const std::string &file) {
        std::lock_guard<std::mutex> locker(m_mtx);
        m_logFileBasename = file;
    }

    std::string GetLogFileBasename() {
        std::lock_guard<std::mutex> locker(m_mtx);
        return m_logFileBasename;
    }

    LogStream& operator<<(const char* str) {
        Append(str, std::strlen(str));
        return *this;
    }

    LogStream& operator<<(char c) {
        Append(c);
        return *this;
    }

    LogStream& operator<<(const std::string& str) {
        Append(str.data(), str.length());
        return *this;
    }

    LogStream& operator<<(int n) {
        auto str = std::to_string(n);
        Append(str.data(), str.length());
        return *this;
    }

    LogStream& operator<<(unsigned int n) {
        auto str = std::to_string(n);
        Append(str.data(), str.length());
        return *this;
    }

    LogStream& operator<<(long n) {
        auto str = std::to_string(n);
        Append(str.data(), str.length());
        return *this;
    }

    LogStream& operator<<(unsigned long n) {
        auto str = std::to_string(n);
        Append(str.data(), str.length());
        return *this;
    }

    LogStream& operator<<(long long n) {
        auto str = std::to_string(n);
        Append(str.data(), str.length());
        return *this;
    }

    LogStream& operator<<(unsigned long long n) {
        auto str = std::to_string(n);
        Append(str.data(), str.length());
        return *this;
    }

    LogStream& operator<<(double n) {
        auto str = std::to_string(n);
        Append(str.data(), str.length());
        return *this;
    }

    void FlushAll();
    void FlushLine();
    void FlushRoll();

private:
    void Append(const char* str, int len) {
        std::lock_guard<std::mutex> locker(m_mtx);
        int n = (len > m_lineBuf.Available()) ? m_lineBuf.Available() : len;
        m_lineBuf.sputn(str, n);
    }

    void Append(char c) {
        std::lock_guard<std::mutex> locker(m_mtx);
        if (m_lineBuf.Available() >= 1) {
            m_lineBuf.sputc(c);
        }
    }

    void Output(const char* msg, int len, bool close = false);
    bool Open();
    void Close();

private:
    std::string m_logFile{nullptr};
    std::string m_logFileBasename{nullptr};
    LogStreamBuf m_lineBuf;
    LogStreamBuf m_buf;
    std::mutex m_mtx;
    std::ofstream m_ofstream;
};

#endif //LOGSTREAM_H
