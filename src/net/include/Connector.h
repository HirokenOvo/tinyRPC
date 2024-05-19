#pragma once

#include "InetAddress.h"
#include "uncopyable.h"
#include <atomic>
#include <functional>
#include <memory>

class Channel;
class EventLoop;

class Connector : uncopyable
{
public:
    using NewConnectionCallback = std::function<void(int)>;

    Connector(EventLoop *loop, const InetAddress &serverAddr);
    ~Connector();

    void setNewConnectionCallback(const NewConnectionCallback &cb);

    void start();   // can be called in any thread
    void restart(); // must be called in loop thread
    void stop();    // can be called in any thread

    const InetAddress &getServerAddress() const;

private:
    enum StateConn
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };
    static const int kMaxRetryDelayMs = 30 * 1000; // 30s
    static const int kInitRetryDelayMs = 500;      // 0.5s

    void setState(StateConn s);
    void startInLoop();
    void stopInLoop();
    void connect();
    void connecting(int sockfd);
    void handleWrite();
    void handleError();
    void retry(int sockfd);
    int removeAndResetChannel();
    void resetChannel();

    EventLoop *loop_;
    InetAddress serverAddr_;
    std::atomic_bool connect_;
    std::atomic_int state_;
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback_;
    int retryDelayMs_;
};