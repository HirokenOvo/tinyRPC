#include "EventLoop.h"
#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <mutex>
#include <sys/eventfd.h>
#include <unistd.h>

// 线程局部变量 防止一个线程创建多个EventLoop
thread_local EventLoop *t_loopInThisThread = nullptr;

// ???定义默认的Poller IO复用接口的超时事件
const int kPollTimeMs = 10000;

/**
 * 创建wakeupfd
 * 放入epool监视 通过使fd可读来唤醒subReactor进入就绪态
 */
int createEventfd()
{
    //  非阻塞模式|执行exec函数时关闭文件描述符确保子进程不会继承该文件描述符
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
        LOG_FATAL("eventfd error:%d \n", errno);

    return evtfd;
}

EventLoop::EventLoop()
    : looping_{false}, threadId_{CurrentThread::getTid()},
      poller_{std::make_unique<EpollPoller>(this)},
      wakeupFd_{createEventfd()}, wakeupChannel_{std::make_unique<Channel>(this, wakeupFd_)},
      callingPendingFunctors_{false}
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    else
        t_loopInThisThread = this;

    // 设置wakeupfd的事件类型以及发生事件后的回调操作
    // ???为什么不用timestamp参数:::bind参数没有占位符 所以std::fuction里参数有多少都无所谓 但是调用的时候要满足std::function声明的参数数量 然后全没用全被忽略了
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}
EventLoop ::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    looping_ = true;
    LOG_INFO("EventLoop %p start looping \n", this);

    while (looping_)
    {
        activeChannels_.clear();
        // 监听client的fd和wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (auto &channel : activeChannels_)
            // Poller监听哪些channel发生事件=>上报给EventLoop=>通知channel处理响应事件
            channel->handleEvent(pollReturnTime_);

        //???已经处理过事件了 这里的functors是什么
        /*
        !!! 其他线程想要调用本线程的函数 简称为 未执行函数
            PendingFunctors是未执行函数的集合,
            wakeup的目的只是加速poll返回,handleEvent处理的是wakup的回调函数而不是未执行函数
            doPendingFunctors才是处理未执行函数
        */
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping. \n", this);
}

void EventLoop::doPendingFunctors()
{
    callingPendingFunctors_ = true;

    std::vector<std::function<void()>> functors;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    for (auto &functor : functors)
        functor();

    callingPendingFunctors_ = false;
}

void EventLoop::quit()
{
    looping_ = false;
    // 如果在其他线程中调用quit 则唤醒线程
    /* ???一个线程只有一个eventloop，a线程怎么调用b线程eventloop的quit
     * subLoop调用了mainLoop的quit,而mainLoop一直没有网络连接堵塞在poll中导致没有及时quit?
     */
    if (!isInLoopThread())
        wakeup();
}

void EventLoop::runInLoop(std::function<void()> cb)
{
    // 在当前loop线程中就执行 否则需要唤醒loop所在线程
    isInLoopThread() ? cb() : queueInLoop(cb);
}
void EventLoop::queueInLoop(std::function<void()> cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    /** 唤醒相应的，需要执行上面回调操作的loop的线程了
     *  ||callingPendingFunctors_的意思是：当前loop正在执行回调，但是loop又有了新的回调
     */
    if (!isInLoopThread() || callingPendingFunctors_)
        wakeup();
}
/**
 * 用来唤醒loop所在的线程的
 * 向wakeupfd_随便写一个数据，wakeupChannel就发生读事件，当前loop线程就会被唤醒
 */
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n == -1)
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n == -1)
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
}

void EventLoop::updateChannel(Channel *channel) { poller_->updateChannel(channel); }
void EventLoop::removeChannel(Channel *channel) { poller_->removeChannel(channel); }
bool EventLoop::hasChannel(Channel *channel) { return poller_->hasChannel(channel); }

bool EventLoop::isInLoopThread() const { return threadId_ == CurrentThread::getTid(); }
