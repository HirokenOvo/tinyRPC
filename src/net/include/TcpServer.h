#pragma once

#include "Acceptor.h"
#include "Buffer.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "uncopyable.h"

#include <functional>
#include <unordered_map>

class EventLoop;

class TcpServer : uncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };
    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const std::string &namePre,
              Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb);
    void setConnectionCallback(const ConnectionCallback &cb);
    void setMessageCallback(const MessageCallback &cb);
    void setWriteCompleteCallback(const WriteCompleteCallback &cb);

    // 设置subLoop数量
    void setThreadNum(int numThreads);

    // 开启服务器监听
    void start();

private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionSPtr &conn);
    void removeConnectionInLoop(const TcpConnectionSPtr &conn);

    EventLoop *loop_; // mainLoop

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_; // 运行在mainLoop,负责监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadPool_;

    std::atomic_int started_;

    int nextConnId_;
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionSPtr>;
    ConnectionMap connections_; // 保存所有连接

    ThreadInitCallback threadInitCallback_;       // loop线程初始化的回调
    ConnectionCallback connectionCallback_;       // 有新连接时的回调
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成之后的回调
};