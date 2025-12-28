//
// Created by asujy on 2025/12/28.
//

#ifndef LOGSTREAMBUF_H
#define LOGSTREAMBUF_H

#include <vector>
#include <streambuf>

class LogStreamBuf : public std::streambuf {
public:
    explicit LogStreamBuf(const int size) : m_bufSize(size) {
        m_buf.resize(size, 0);
        Reset();
    }

    void Reset() {
        setp(m_buf.data(), m_buf.data() + m_bufSize - 1);
    }

    // 覆写streambuf中的函数
    int_type overflow(int_type c) override {
        return c;
    }

    const char* BasePtr() const {
        return pbase();
    }

    const char* CurPtr() const {
        return pptr();
    }

    int Used() const {
        return pptr() - pbase();
    }

    int Available() const {
        return epptr() - pptr();
    }

private:
    const int m_bufSize;
    std::vector<char> m_buf;
};

#endif //LOGSTREAMBUF_H
