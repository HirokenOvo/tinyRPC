#include "include/EventLoopThread.h"
#include "include/EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string &name)
    : loop_{nullptr}, exiting_{false},
      thread_{std::bind(&EventLoopThread::threadFunc, this), name},
      mutex_{},
      cond_{},
      callback_{cb}
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        thread_.join(); // 先等待子线程运行完毕
        loop_->quit();  // 退出事件循环
    }
}

EventLoop *EventLoopThread::startLoop()
{
    thread_.start(); //->建立新线程,执行func_(),即EventLoopThread::threadFunc函数

    EventLoop *loop = nullptr;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr)
            cond_.wait(lock);

        loop = loop_;
    }
    return loop;
}
// 在Thread类中新建的线程里运行
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 在新线程中开启一个新的EventLoop(one loop per thread)

    if (callback_)
        callback_(&loop);

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one(); // 唤醒EventLoopThread::startLoop,传递EventLoop
    }

    loop.loop(); // 开始事件循环,通过Poller.poll获取事件

    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr; // loop.loop()结束,nullptr标识该线程的事件循环已结束
}