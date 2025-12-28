//
// Created by asujy on 2025/12/28.
//

#include "log/LogStream.h"

void LogStream::FlushLine() {
    std::lock_guard<std::mutex> locker(m_mtx);
    if (m_buf.Available() < m_lineBuf.Used()) {
        Output(m_buf.BasePtr(), m_buf.Used());
        m_buf.Reset();
    }
    m_buf.sputn(m_lineBuf.BasePtr(), m_lineBuf.Used());
    m_lineBuf.Reset();
}

void LogStream::FlushAll() {
    std::lock_guard<std::mutex> locker(m_mtx);
    if (m_buf.Used() > 0) {
        Output(m_buf.BasePtr(), m_buf.Used());
    }
    m_buf.Reset();
}

void LogStream::Output(const char *msg, int len) {
    std::unique_lock<std::mutex> locker(m_mtx);
    if (m_logFile.empty()) {
        locker.unlock();
        return;
    }
    locker.unlock();
    if (!Open()) {
        return;
    }
    {
        locker.lock();
        // 写入数据到文件，flush确保数据刷到磁盘
        m_ofstream.write(msg, len);
        m_ofstream.flush();
        locker.unlock();
    }
}

bool LogStream::Open() {
    std::lock_guard<std::mutex> locker(m_mtx);
    if (m_ofstream.is_open()) {
        return true;
    }
    m_ofstream.open(m_logFile, std::ostream::out | std::ostream::app |
                        std::ostream::binary);
    if (!m_ofstream.is_open()) {
        throw std::runtime_error("Failed to open log file: " + m_logFile);
    }
    return true;
}

void LogStream::Close() {
    std::lock_guard<std::mutex> locker(m_mtx);
    if (m_ofstream.is_open()) {
        m_ofstream.close();
    }
}




