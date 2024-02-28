#include <sys/epoll.h>

#include "include/Channel.h"
#include "include/Logger.h"
#include "include/EventLoop.h"

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_{loop}, fd_{fd}, events_{kNoneEvent}, revents_{kNoneEvent}, index_{-1},
      tied_{false}
{
}

void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

// fd得到poller通知之后处理事件的函数
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> lock = tie_.lock();
        if (lock)
            handleEventWithGuard(receiveTime);
    }
    else
        handleEventWithGuard(receiveTime);
}

// 根据poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n", revents_);
    //???为什么不用清空回调函数
    //!!!回调函数是每次发生对应事件都要执行的 不是一次性的

    // 发生了挂起事件且没有可读事件且有相应回调函数
    if ((revents_ & EPOLLHUP) && !(revents_ & kReadEvent) && closeCallback_)
        // 如果没有tie_,可能程序执行到这就已经释放了Channel对象,继续执行会引发ub
        closeCallback_();

    // 发生了错误事件
    if ((revents_ & EPOLLERR) && errorCallback_)
        errorCallback_();

    // 发生了可读事件或紧急可读数据事件
    if ((revents_ & kReadEvent) && readCallback_)
        readCallback_(receiveTime);

    // 发生了可写事件
    if ((revents_ & kWriteEvent) && writeCallback_)
        writeCallback_();
}

// 设置fd相应的事件状态
void Channel::enableReading() { events_ |= kReadEvent, update(); }
void Channel::enableWriting() { events_ |= kWriteEvent, update(); }
void Channel::disableReading() { events_ &= ~kReadEvent, update(); }
void Channel::disableWriting() { events_ &= ~kWriteEvent, update(); }
void Channel::disableAll() { events_ = kNoneEvent, update(); }

// 通过channel所属的EventLoop，调用poller相应方法,注册/移除相应事件
void Channel::update() { loop_->updateChannel(this); }
void Channel::remove() { loop_->removeChannel(this); }

int Channel::getFd() const { return fd_; }
int Channel::getEvents() const { return events_; }
void Channel::setRevents(int revt) { revents_ = revt; }

int Channel::getIndex() { return index_; }
void Channel::setIndex(int idx) { index_ = idx; }
// one loop per thread
EventLoop *Channel::ownerLoop() { return loop_; }

// 设置回调函数 除wakeupChannel外绑定的是TcpConnection::handleXXX()
void Channel::setReadCallback(std::function<void(Timestamp)> &&cb) { readCallback_ = std::move(cb); }
void Channel::setWriteCallback(std::function<void()> &&cb) { writeCallback_ = std::move(cb); }
void Channel::setCloseCallback(std::function<void()> &&cb) { closeCallback_ = std::move(cb); }
void Channel::setErrorCallback(std::function<void()> &&cb) { errorCallback_ = std::move(cb); }

// 返回fd当前的事件状态
bool Channel::isNoneEvent() const { return events_ == kNoneEvent; }
bool Channel::canWriting() const { return events_ & kWriteEvent; }
bool Channel::canReading() const { return events_ & kReadEvent; }