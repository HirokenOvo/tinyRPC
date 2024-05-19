#include "EventLoop.h"
#include "TcpClient.h"
#include "Connector.h"
#include "Callbacks.h"

#include <strings.h>
namespace tmp
{
    void removeConnection(EventLoop *loop, const TcpConnectionSPtr &conn)
    {
        loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

TcpClient::TcpClient(EventLoop *loop, const InetAddress &serverAddr, const std::string &nameArg)
    : loop_{loop}, connector_{std::make_shared<Connector>(loop, serverAddr)}, name_{nameArg},
      retry_{false}, connect_{true}, nextConnId_{1}
{
    connector_->setNewConnectionCallback(std::bind(&TcpClient::newConnection, this, std::placeholders::_1));
}

TcpClient::~TcpClient()
{
    TcpConnectionSPtr conn;
    bool unique = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        unique = connection_.unique();
        conn = connection_;
    }
    if (conn)
    {
        // FIXME: not 100% safe, if we are in different thread
        CloseCallback cb = std::bind(&tmp::removeConnection, loop_, std::placeholders::_1);
        loop_->runInLoop(std::bind(&TcpConnection::setCloseCallback, conn, cb));
        if (unique)
            conn->forceClose();
        else
            connector_->stop();
    }
}

void TcpClient::connect()
{
    connect_ = true;
    connector_->start();
}
void TcpClient::disconnect()
{
    connect_ = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (connection_)
            connection_->shutdown();
    }
}
void TcpClient::stop()
{
    connect_ = false;
    connector_->stop();
}

TcpConnectionSPtr TcpClient::getConnecction()
{
    std::unique_lock<std::mutex> lock(mutex_);
    return connection_;
}
EventLoop *TcpClient::getLoop() const { return loop_; }
bool TcpClient::getRetry() const { return retry_; }
void TcpClient::enableRetry() { retry_ = true; }

const std::string &TcpClient::getName() const { return name_; }
// Not thread safe.
void TcpClient::setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
void TcpClient::setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
void TcpClient::setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }

static struct sockaddr_in getLocalAddr(int sockfd)
{
    struct sockaddr_in localaddr;
    bzero(&localaddr, sizeof(localaddr));
    socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
    if (::getsockname(sockfd, static_cast<struct sockaddr *>(static_cast<void *>(&localaddr)), &addrlen) < 0)
    {
        std::cout << "[ERROR]:sockets::getLocalAddr\n";
    }
    return localaddr;
}

static struct sockaddr_in getPeerAddr(int sockfd)
{
    struct sockaddr_in peeraddr;
    bzero(&peeraddr, sizeof(peeraddr));
    socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);
    if (::getpeername(sockfd, static_cast<struct sockaddr *>(static_cast<void *>(&peeraddr)), &addrlen) < 0)
    {
        std::cout << "[ERROR]:sockets::getPeerAddr\n";
    }
    return peeraddr;
}

// Not thread safe, but in loop
void TcpClient::newConnection(int sockfd)
{
    InetAddress peerAddr(getPeerAddr(sockfd));
    char buf[32];
    snprintf(buf, sizeof buf, ":%s#%d", peerAddr.toIpPort().c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    InetAddress localAddr(getLocalAddr(sockfd));
    // FIXME poll with zero timeout to double confirm the new connection
    // FIXME use make_shared if necessary
    TcpConnectionSPtr conn(
        new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr));

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(
        std::bind(&TcpClient::removeConnection, this, std::placeholders::_1)); // FIXME: unsafe
    {
        std::unique_lock<std::mutex> lock(mutex_);
        connection_ = conn;
    }
    conn->connectEstablished();
}
// Not thread safe, but in loop
void TcpClient::removeConnection(const TcpConnectionSPtr &conn)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        connection_.reset();
    }
    loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    if (retry_ && connect_)
    {
        std::cout << "TcpClient::connect[" << name_ << "] - Reconnecting to "
                  << connector_->getServerAddress().toIpPort() << std::endl;
        connector_->restart();
    }
}
