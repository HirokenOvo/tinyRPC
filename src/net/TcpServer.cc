#include "include/TcpServer.h"
#include "include/Acceptor.h"
#include "include/Callbacks.h"
#include "include/Logger.h"

#include <memory>
#include <strings.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);

    return loop;
}

TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const std::string &namePre,
                     Option option)
    : loop_{CheckLoopNotNull(loop)},
      ipPort_{listenAddr.toIpPort()}, name_{namePre},
      acceptor_{std::make_unique<Acceptor>(loop_, listenAddr, option == kReusePort)},
      threadPool_{std::make_shared<EventLoopThreadPool>(loop_, namePre)},
      started_{0},
      nextConnId_{1}
{
    // 有用户连接时,会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                                  std::placeholders::_1, std::placeholders::_2));
}
TcpServer::~TcpServer()
{
    for (auto &[connName, connSPtr] : connections_)
    {
        TcpConnectionSPtr conn(std::move(connSPtr));

        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// 开启服务器监听 loop.loop()
void TcpServer::start()
{
    if (started_++ == 0) // 防止TcpServer被start多次
    {
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // 轮询选择一个subLoop来管理channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_++);

    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本地ip地址和端口信息
    sockaddr_in local;
    socklen_t addrlen = sizeof local;
    bzero(&local, addrlen);
    if (getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
        LOG_ERROR("sockets::getLocalAddr");

    InetAddress localAddr{local};

    // 根据连接成功的sockfd,创建TcpConnection连接对象
    TcpConnectionSPtr conn = std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddr, peerAddr);
    connections_[connName] = conn;

    // 用户设置给TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置如何关闭连接的回调 conn->shutDown()
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 直接调用TcpConnection::connectEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}
void TcpServer::removeConnection(const TcpConnectionSPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}
void TcpServer::removeConnectionInLoop(const TcpConnectionSPtr &conn)
{
    connections_.erase(conn->getName());
    conn->getLoop()->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}

void TcpServer::setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
void TcpServer::setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
void TcpServer::setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
void TcpServer::setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

// 设置subLoop数量
void TcpServer::setThreadNum(int numThreads) { threadPool_->setThreadNum(numThreads); }