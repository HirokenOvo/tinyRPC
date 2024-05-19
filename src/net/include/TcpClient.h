#pragma once

#include "TcpConnection.h"
#include "uncopyable.h"

#include <mutex>
#include <atomic>

class Connector;

class TcpClient : uncopyable
{
public:
    TcpClient(EventLoop *loop, const InetAddress &serverAddr, const std::string &nameArg);
    ~TcpClient();

    void connect();
    void disconnect();
    void stop();

    TcpConnectionSPtr getConnecction();
    EventLoop *getLoop() const;
    bool getRetry() const;
    void enableRetry();

    const std::string &getName() const;
    // Not thread safe.
    void setConnectionCallback(ConnectionCallback cb);
    void setMessageCallback(MessageCallback cb);
    void setWriteCompleteCallback(WriteCompleteCallback cb);

private:
    // Not thread safe, but in loop
    void
    newConnection(int sockfd);
    // Not thread safe, but in loop
    void removeConnection(const TcpConnectionSPtr &conn);

    using ConnectorSPtr = std::shared_ptr<Connector>;
    EventLoop *loop_;
    ConnectorSPtr connector_;
    const std::string name_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    std::atomic_bool retry_;
    std::atomic_bool connect_;
    // always in loop thread
    int nextConnId_;
    std::mutex mutex_;
    TcpConnectionSPtr connection_;
};