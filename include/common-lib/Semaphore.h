//
// Created by asujy on 2025/12/31.
//

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <condition_variable>

class Semaphore {
public:
    explicit Semaphore(int count) : m_count(count < 0 ? 0 :count) {}
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    Semaphore(Semaphore&&) = delete;
    Semaphore& operator=(Semaphore&&) = delete;

    virtual ~Semaphore() {}

    void Post();
    void Wait();

private:
    std::mutex m_mtx;
    std::condition_variable m_condv;
    int m_count = 0;
};

#endif //SEMAPHORE_H
