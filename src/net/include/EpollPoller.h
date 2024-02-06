#pragma once

#include "uncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>
#include <sys/epoll.h>

class Channel;
class EventLoop;

class EpollPoller : uncopyable
{
public:
    using ChannelList = std::vector<Channel *>;
    EpollPoller(EventLoop *loop);
    ~EpollPoller();

    Timestamp poll(int timeoutMs, ChannelList *activeChannels);
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);

    // 判断channel是否在当前Poller中
    bool hasChannel(Channel *channel) const;

private:
    // 定义Poller所属的事件循环EventLoop
    EventLoop *ownerLoop_;
    //[ sockfd , sockfd所属的channel通道类型 ]
    std::unordered_map<int, Channel *> channels_;

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel通道
    void update(int operation, Channel *channel);

    static const int kInitEventListSize = 16;
    using EventList = std::vector<epoll_event>;
    // epoll实例
    int epollfd_;
    // 保存就绪事件的数组
    EventList events_;
};