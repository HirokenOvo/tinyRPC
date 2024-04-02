#pragma once

#include "../../comm/include/uncopyable.h"

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <atomic>

class Thread : uncopyable
{
public:
    explicit Thread(std::function<void()> &&func, const std::string &name = std::string{});
    ~Thread();

    void start();
    void join();

    bool getStarted() const;
    pid_t getTid() const;
    const std::string &getName() const;

    static int getNumCreated();

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    std::function<void()> func_;
    std::string name_;
    static std::atomic_int numCreated_;
};