#pragma once

#include "uncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : uncopyable
{
public:
    using NewConnectionCallBack = std::function<void(int sockfd, const InetAddress &)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallBack &cb);

    bool isListenning() const;
    void listen();

private:
    void handleRead();

    EventLoop *loop_; // mainLoop
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallBack newConnectionCallBack_;
    bool listenning_;
};