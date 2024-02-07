#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>

#include "uncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class EpollPoller;

class EventLoop : uncopyable
{
public:
    EventLoop();
    ~EventLoop();

    //  开启事件循环
    void loop();
    //  退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 在当前loop中执行callback
    void runInLoop(std::function<void()> cb);
    // 将cb放入队列中，唤醒loop所在的线程，执行callback
    void queueInLoop(std::function<void()> cb);

    // 用来唤醒loop所在线程
    void wakeup();

    // 通过调用poller的方法执行
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    bool isInLoopThread() const;

private:
    // wake up
    void handleRead();
    // 执行回调
    void doPendingFunctors();

    std::atomic_bool looping_;

    const pid_t threadId_; // 当前loop的线程id

    Timestamp pollReturnTime_; // poller返回发生事件的channels的时间点
    std::unique_ptr<EpollPoller> poller_;

    /**
     * 当mainLoop获取一个新用户channel,
     * 通过轮询算法选择一个subloop,
     * 通过该成员唤醒subloop处理channel
     */
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    std::vector<Channel *> activeChannels_;

    std::atomic_bool callingPendingFunctors_;            // 标识当前loop是否有需要执行回调的操作
    std::vector<std::function<void()>> pendingFunctors_; // 存储loop需要执行的所有回调操作
    std::mutex mutex_;                                   // 保护pendingFunctors线程安全
};
