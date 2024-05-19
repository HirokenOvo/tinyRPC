#include "TcpServer.h"
#include "Acceptor.h"
#include "Callbacks.h"
#include "Logger.h"

#include <memory>
#include <strings.h>
#include <signal.h>
#include <mutex>

class InterruptibleSleeper
{
public:
    // returns false if killed:
    void wait()
    {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock);
    }

    void interrupt()
    {
        std::unique_lock<std::mutex> lock(m);
        cv.notify_all();
    }

private:
    std::condition_variable cv;
    std::mutex m;
};
static InterruptibleSleeper InterruptHandler;

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
      started_{false},
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

static void watchdog(int signal)
{
    if (signal == SIGTERM || signal == SIGINT)
    {
        std::cout << "Received signal " << signal << ", shutting down gracefully..." << std::endl;
        InterruptHandler.interrupt();
    };
}
// 开启服务器监听 loop.loop()
void TcpServer::start()
{
    if (!started_) // 防止TcpServer被start多次
    {
        struct sigaction sa;
        sa.sa_handler = watchdog;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGTERM, &sa, nullptr);
        sigaction(SIGINT, &sa, nullptr);

        started_ = true;
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));

        std::thread t([]()
                      {InterruptHandler.wait();
        // TODO:优雅关闭。拒绝所有新连接，已有连接请求超过10s后强制返回退出
        // loop_->quit();
        exit(0); });
        t.detach();
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
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
    // 轮询选择一个subLoop来管理channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    // 根据连接成功的sockfd,创建TcpConnection连接对象
    TcpConnectionSPtr conn = std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddr, peerAddr);
    connections_[connName] = conn;

    // 用户设置给TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置如何关闭连接的回调 conn->connectDestroyed()
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