#include "include/EpollPoller.h"
#include "include/Channel.h"
#include "include/Logger.h"

#include <strings.h>
#include <unistd.h>

EpollPoller::EpollPoller(EventLoop *loop)
    : ownerLoop_{loop}, epollfd_{epoll_create1(EPOLL_CLOEXEC)},
      events_{kInitEventListSize}
{
    if (epollfd_ < 0)
        LOG_FATAL("epoll_create error:%d \n", errno);
}

EpollPoller::~EpollPoller()
{
    close(epollfd_);
}

bool EpollPoller::hasChannel(Channel *channel) const
{
    auto idx = channels_.find(channel->getFd());
    return idx != channels_.end() && idx->second == channel;
}

Timestamp EpollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    int numEvents = epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size())
            events_.reserve(events_.size() * 2);
    }
    else if (!numEvents)
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    else
    {
        if (saveErrno != EINTR)
        {
            // 处理错误情况，EINTR 表示被信号中断，不视为错误
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

// channel未添加到poller中
const int kNone = -1;
// channel已添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

// channel::update/remove => EventLoop::updateChannel/removeChannel=>Poller::updateChannel/removeChannel
void EpollPoller::updateChannel(Channel *channel)
{
    const int index = channel->getIndex();

    if (index == kNone || index == kDeleted)
    {
        if (index == kNone)
        {
            int fd = channel->getFd();
            channels_[fd] = channel;
        }
        channel->setIndex(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // channel已在poller上注册过了
    {
        int fd = channel->getFd();
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->setIndex(kDeleted);
        }
        else
            update(EPOLL_CTL_MOD, channel);
    }
}

// 从poller中删除channel
void EpollPoller::removeChannel(Channel *channel)
{
    const int fd = channel->getFd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    const int index = channel->getIndex();
    if (index == kAdded)
        update(EPOLL_CTL_DEL, channel);
    channel->setIndex(kNone);
}

// 填写活跃的连接
void EpollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; i++)
    {
        /**
         * struct epoll_event {
         *   uint32_t events;        // 事件类型，表示注册的事件和发生的事件
         *   epoll_data_t data;      // 用户数据或者文件描述符
         * };
         * typedef union epoll_data {
         *      void        *ptr;
         *      int          fd;
         *      uint32_t     u32;
         *      uint64_t     u64;
         *  } epoll_data_t;
         */
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->setRevents(events_[i].events);
        activeChannels->emplace_back(channel);
    }
}

// 更新channel通道 epoll_ctl add/mod/del
void EpollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);

    int fd = channel->getFd();

    event.events = channel->getEvents();
    event.data.fd = fd;
    event.data.ptr = channel;

    if (epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        else
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
    }
}