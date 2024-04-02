#pragma once

#include "../../comm/include/uncopyable.h"
#include "../../comm/include/Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;
/** 事件处理器.管理一个fd(并不负责fd的生命周期),并对事件进行封装
 *  具有在Epoller上注册fd感兴趣的功能+事件发生后执行相应的回调
 *  fd可以是socket、eventfd等
 */
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

    // 返回fd当前的事件状态 无事件;可写;可读
    bool isNoneEvent() const;
    bool canWriting() const;
    bool canReading() const;

    int getIndex();
    void setIndex(int idx);

    // one loop per thread
    EventLoop *ownerLoop();

    void remove();

private:
    void update();
    // 根据poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;  // 表示什么事件都不关心
    static const int kReadEvent;  // 关注可读事件和紧急可读数据
    static const int kWriteEvent; // 关注可写事件

    EventLoop *loop_; // 事件循环
    const int fd_;    // Channel管理的文件描述符 Poller监听的对象
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // poller返回的具体发生事件
    int index_;

    /**
     * 在没有tie_的时候，当客户端主动关闭连接时，服务器上该连接对应的sockfd上就会有事件发生，
     * 也就是该sockfd对应的Channel会调用Channel::handleEvent()来关闭连接，
     * 关闭连接就会释放释放TcpConnection，进而Channel对象也会析构，
     * 这就造成了Channel::handleEvent()还只执行到一半，Channel就已经被析构了，引发不可预测的后果，
     * 为了避免这个问题，可以使用弱指针tie_绑定到TcpConnection的共享指针上，如果tie_能够被转化为TcpConnection共享指针，
     * 这就延长了TcpConnection的生命周期，使之长过Channel::handleEvent()，
     * 这样一来，Channel::handleEvent()来关闭连接时，
     * 下面代码的guard变量依然持有一份TcpConnection，使其的引用计数不会减为0，
     * 也就是说Channel不会在执行完Channel::handleEvent()之前被析构
     */
    std::weak_ptr<void> tie_;
    bool tied_;

    // channel中能够获得fd最终发生的具体时间的revents，因此它负责调用具体事件的回调函数
    std::function<void(Timestamp)> readCallback_;
    std::function<void()> writeCallback_;
    std::function<void()> closeCallback_;
    std::function<void()> errorCallback_;
};