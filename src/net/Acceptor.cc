#include "Logger.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include <sys/socket.h>
#include <unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_{loop}, acceptSocket_{createNonblocking()},
      acceptChannel_{loop, acceptSocket_.getFd()}, listenning_{false}
{
    acceptSocket_.setReuseAddr(true);      // 套接字关闭后能立即重用相同端口
    acceptSocket_.setReusePort(reuseport); // 多个套接字绑定同一端口
    acceptSocket_.bindAddress(listenAddr); // 绑定地址

    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}
// 绑定TcpServer::newConnection
void Acceptor::setNewConnectionCallback(const NewConnectionCallBack &cb) { newConnectionCallBack_ = cb; }

bool Acceptor::isListenning() const { return listenning_; }
void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead()
{
    InetAddress peerAddr; // 客户端地址
    int sockfd = acceptSocket_.accept(&peerAddr);
    if (sockfd >= 0)
    {
        if (newConnectionCallBack_)
            newConnectionCallBack_(sockfd, peerAddr); // 轮询找到subLoop，唤醒，分发当前的新客户端的Channel???轮询在哪里!!!newConnectionCallBack(TcpServer::newConnection)里
        else
            ::close(sockfd);
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}