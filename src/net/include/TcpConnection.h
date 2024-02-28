#pragma once

#include "uncopyable.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <atomic>

class Channel;
class EventLoop;
class Socket;

// 管理连接connChannel,
class TcpConnection : uncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                  const std::string &namePre,
                  int sockfd,
                  const InetAddress localAddr,
                  const InetAddress peerAddr);
    ~TcpConnection();

    EventLoop *getLoop() const;
    const std::string getName() const;
    const InetAddress getLocalAddress() const;
    const InetAddress getPeerAddress() const;

    bool isConnected() const;

    // 发送数据
    void send(const std::string &message);
    // 关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback &cb);
    void setMessageCallback(const MessageCallback &cb);
    void setWriteCompleteCallback(const WriteCompleteCallback &cb);
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark);
    void setCloseCallback(const CloseCallback &cb);

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

private:
    enum StateConn
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };
    void setState(const StateConn state);

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void *message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_; // subLoop,TcpConnection是在subLoop中管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;       // 有连接状态有变化的回调
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_; // 缓冲区的高水位线,当缓冲区中的数据量达到或超过这个标记时，触发高水位标记回调函数

    Buffer inputBuffer_;  // 接收数据的缓冲区
    Buffer outputBuffer_; // 发送数据的缓冲区
};