//
// Created by asujy on 2025/12/31.
//

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <list>
#include "common-lib/Semaphore.h"

template <typename T>
class ThreadPool {
public:
    ThreadPool(int threadNumber = 8, int maxRequest = 10000);
    ~ThreadPool();

    bool Append(T *request);
private:
    static void* Worker(ThreadPool *pool);
    void Run();
private:
    int m_threadNumber{0};
    std::vector<std::thread> m_threads;
    int m_maxRequests{0};
    std::list<T*> m_workQueue;
    std::mutex m_queueLocker;
    Semaphore m_queueStat;
    std::atomic<bool> m_stop{false};
};

template <typename T>
ThreadPool<T>::ThreadPool(int threadNumber, int maxRequest) :
    m_threadNumber(threadNumber), m_maxRequests(maxRequest), m_queueStat(0) {
        if (m_threadNumber <= 0 || m_maxRequests <= 0) {
            LOG_ERROR << "Threadpool constructor: threadNumber and "
                         "maxRequests must be positive";
            std::exit(EXIT_FAILURE);
        }
    m_threads.reserve(m_threadNumber);
    for (int i = 0; i < m_threadNumber; ++i) {
        try {
            m_threads.emplace_back(Worker, this);
            m_threads.back().detach();
            LOG_DEBUG << "create the " << i << "th thread";
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("create thread failed: ") + e.what());
        }
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool() {
    m_stop.store(true);
    for (int i = 0; i < m_threadNumber; ++i) {
        m_queueStat.Post();
    }
    std::lock_guard<std::mutex> locker(m_queueLocker);
    if (!m_workQueue.empty()) {
        LOG_ERROR << "threadpool destroyed with" << m_workQueue.size()
                    << " unprocessed tasks";
    }
}

template <typename T>
bool ThreadPool<T>::Append(T* request) {
    if (request == nullptr) {
        LOG_ERROR << "ThreadPool::Append(): append null task to threadpool!!!";
        return false;
    }
    std::lock_guard<std::mutex> locker(m_queueLocker);
    if (static_cast<int>(m_workQueue.size()) >= m_maxRequests) {
        LOG_WARN << "ThreadPool::Append(): threadpool task queue "
                    "is full (append failed)!!!";
        return false;
    }
    m_workQueue.emplace_back(request);
    m_queueStat.Post();
    return true;
}

template <typename T>
void* ThreadPool<T>::Worker(ThreadPool* pool) {
    if (pool == nullptr) {
        return nullptr;
    }
    pool->Run();
    return pool;
}

template <typename T>
void ThreadPool<T>::Run() {
    while (!m_stop.load() || !m_workQueue.empty()) {
        m_queueStat.Wait();
        std::unique_lock<std::mutex> locker(m_queueLocker);
        if (m_workQueue.empty()) {
            continue;
        }

        T* request = m_workQueue.front();
        m_workQueue.pop_front();
        if (request != nullptr) {
            request->Process();
        }
    }
}

#endif //THREADPOOL_H
