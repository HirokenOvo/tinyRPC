#pragma once

#include "uncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;

class Channel : uncopyable
{
public:
    Channel(EventLoop *loop, int fd);
    ~Channel() = default;

    // fd得到poller通知之后处理事件的函数
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数
    //???为什么readcallback参数需要加Timestamp
    void setReadCallback(std::function<void(Timestamp)> &&cb);
    void setWriteCallback(std::function<void()> &&cb);
    void setCloseCallback(std::function<void()> &&cb);
    void setErrorCallback(std::function<void()> &&cb);

    /**
     * 当一个TcpConnection新连接创建时，将新连接和channel所绑定
     * 防止channel还在执行回调操作时被手动remove掉
     */
    void tie(const std::shared_ptr<void> &obj);

    int getFd() const;
    int getEvents() const;
    void setRevents(int revt);

    // 设置fd相应的事件状态
    void enableReading();
    void enableWriting();
    void disableReading();
    void disableWriting();
    void disableAll();

    // 返回fd当前的事件状态
    bool isNoneEvent() const;
    bool isWriting() const;
    bool isReading() const;

    int getIndex();
    void setIndex(int idx);

    // one loop per thread
    EventLoop *ownerLoop();

    void remove();

private:
    void update();
    // 根据poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;  // 表示无事件
    static const int kReadEvent;  // 关注可读事件和紧急可读数据
    static const int kWriteEvent; // 关注可写事件

    EventLoop *loop_; // 事件循环
    const int fd_;    // fd、Poller监听的对象
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // poller返回的具体发生事件
    int index_;

    /**
     * 避免channel对象被销毁而对应的回调函数仍在运行从而引发悬挂指针问题
     * 确保在处理事件的回调函数时相关资源仍然有效
     */
    std::weak_ptr<void> tie_;
    bool tied_;

    // channel中能够获得fd最终发生的具体时间的revents，因此它负责调用具体事件的回调函数
    std::function<void(Timestamp)> readCallback_;
    std::function<void()> writeCallback_;
    std::function<void()> closeCallback_;
    std::function<void()> errorCallback_;
};