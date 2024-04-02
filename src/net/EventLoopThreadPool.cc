#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include <memory>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &namePre)
    : baseLoop_{baseLoop}, name_{namePre},
      started_{false}, numThreads_{0}, next_{0}
{
}

void EventLoopThreadPool::setThreadNum(int numThreads) { numThreads_ = numThreads; }

void EventLoopThreadPool::start(const ThreadInitCallBack &cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; i++) // 添加子线程
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);

        std::unique_ptr<EventLoopThread> t = std::make_unique<EventLoopThread>(cb, std::string{buf});
        threads.emplace_back(std::move(t));
        loops_.push_back(threads.back()->startLoop());
    }
    // 整个服务端只有一个线程，运行着baseloop
    if (!numThreads_ && cb)
        cb(baseLoop_);
}

// 如果工作在多线程中，baseLoop_以轮询的方式分配channel给subloop
EventLoop *EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_;

    if (!loops_.empty())
    {
        loop = loops_[next_++];
        if (next_ >= loops_.size())
            next_ = 0;
    }
    return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
        return std::vector<EventLoop *>{baseLoop_};
    return loops_;
}

bool EventLoopThreadPool::getStarted() const { return started_; }
const std::string EventLoopThreadPool::getName() const { return name_; }