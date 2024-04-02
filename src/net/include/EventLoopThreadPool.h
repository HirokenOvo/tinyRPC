#pragma once

#include "EventLoopThread.h"
#include "../../comm/include/uncopyable.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : uncopyable
{
public:
    using ThreadInitCallBack = std::function<void(EventLoop *)>;
    EventLoopThreadPool(EventLoop *baseLoop, const std::string &namePre);
    ~EventLoopThreadPool() = default;

    void setThreadNum(int numThreads);

    void start(const ThreadInitCallBack &cb = ThreadInitCallBack());

    // 如果工作在多线程中，baseLoop_以轮询的方式分配channel给subloop
    EventLoop *getNextLoop();

    std::vector<EventLoop *> getAllLoops();
    bool getStarted() const;
    const std::string getName() const;

private:
    EventLoop *baseLoop_;
    std::string name_;
    bool started_;
    int numThreads_; // 线程池子线程数量
    int next_;       // 轮询下标
    std::vector<std::unique_ptr<EventLoopThread>> threads;
    std::vector<EventLoop *> loops_;
};