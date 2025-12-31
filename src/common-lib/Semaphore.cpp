//
// Created by asujy on 2025/12/31.
//

#include "common-lib/Semaphore.h"

void Semaphore::Post() {
    std::lock_guard<std::mutex> locker(m_mtx);
    ++m_count;
    m_condv.notify_one();
}

void Semaphore::Wait() {
    std::unique_lock<std::mutex> locker(m_mtx);
    while (m_count <= 0) {
        m_condv.wait(locker);
    }
    --m_count;
}

