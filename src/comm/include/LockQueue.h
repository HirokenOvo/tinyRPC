#pragma once

#include <mutex>
#include <queue>
#include <condition_variable>

template <typename T>
class LockQueue
{
public:
    void push(const T &data)
    {
        std::scoped_lock lock(mutex_);
        qe_.push(data);
        cond_.notify_one();
    }
    T pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (qe_.empty())
            cond_.wait(lock);

        T data = qe_.front();
        qe_.pop();
        return data;
    }

    T front()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (qe_.empty())
            cond_.wait(lock);

        return qe_.front();
    }

private:
    std::mutex mutex_;
    std::condition_variable cond_;
    std::queue<T> qe_;
};