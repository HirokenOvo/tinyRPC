#include "Connector.h"
#include "Channel.h"
#include "EventLoop.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

Connector::Connector(EventLoop *loop, const InetAddress &serverAddr)
    : loop_{loop}, serverAddr_{serverAddr},
      connect_{false}, state_{kDisconnected}, retryDelayMs_{kInitRetryDelayMs}
{
    std::cout << "ctor[" << this << "]\n";
}

Connector::~Connector()
{
    std::cout << "dtor[" << this << "]\n";
}

// can be called in any thread
void Connector::start()
{
    connect_ = true;
    loop_->runInLoop(std::bind(&Connector::startInLoop, this));
}

// must be called in loop thread
void Connector::restart()
{
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

void Connector::startInLoop()
{
    if (connect_)
        connect();
    else
        std::cout << this << " do not connect\n";
}

// can be called in any thread
void Connector::stop()
{
    connect_ = false;
    loop_->queueInLoop(std::bind(&Connector::stopInLoop, this)); // FIXME: unsafe
}

void Connector::stopInLoop()
{
    if (state_ == kConnecting)
    {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::connect()
{
    int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    int ret = ::connect(sockfd, (struct sockaddr *)(serverAddr_.getSockAddr()), sizeof(*(serverAddr_.getSockAddr())));
    int savedErrno = (ret == 0) ? 0 : errno;
    switch (savedErrno)
    {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
        connecting(sockfd);
        break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
        retry(sockfd);
        break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
        std::cout << "connect error in Connector::startInLoop " << savedErrno << std::endl;
        ::close(sockfd);
        break;

    default:
        std::cout << "Unexpected error in Connector::startInLoop " << savedErrno << std::endl;
        ::close(sockfd);
        // connectErrorCallback_();
        break;
    }
}

void Connector::connecting(int sockfd)
{
    setState(kConnecting);
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback(
        std::bind(&Connector::handleWrite, this)); // FIXME: unsafe
    channel_->setErrorCallback(
        std::bind(&Connector::handleError, this)); // FIXME: unsafe
    channel_->enableWriting();
}

static int getSocketError(int sockfd)
{
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);
    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
        return errno;
    else
        return optval;
}

void Connector::handleWrite()
{
    std::cout << "Connector::handleWrite " << state_ << std::endl;

    if (state_ == kConnecting)
    {
        int sockfd = removeAndResetChannel();
        int err = getSocketError(sockfd);
        if (err)
        {
            std::cout << "Connector::handleWrite - SO_ERROR = " << err << std::endl;

            retry(sockfd);
        }
        // FIXME: selfConnect
        //  else if (isSelfConnect(sockfd))
        //  {
        //      std::cout << "Connector::handleWrite - Self connect" << std::endl;

        //     retry(sockfd);
        // }
        else
        {
            setState(kConnected);
            if (connect_)
            {
                newConnectionCallback_(sockfd);
            }
            else
            {
                close(sockfd);
            }
        }
    }
    else
    {
        // what happened?
    }
}

void Connector::handleError()
{
    std::cout << "Connector::handleError state=" << state_ << std::endl;
    if (state_ == kConnecting)
    {
        int sockfd = removeAndResetChannel();
        int err = getSocketError(sockfd);
        std::cout << "SO_ERROR = " << err << std::endl;
        retry(sockfd);
    }
}

void Connector::retry(int sockfd)
{
    ::close(sockfd);
    setState(kDisconnected);
    if (connect_)
    {
        std::cout << "Connector::retry - Retry connecting to "
                  << serverAddr_.toIpPort() << " in " << retryDelayMs_
                  << " milliseconds. \n";
        // loop_->runAfter(retryDelayMs_ / 1000.0,
        //                 std::bind(&Connector::startInLoop, shared_from_this()));
        // retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    }
    else
        std::cout << "do not connect\n";
}

int Connector::removeAndResetChannel()
{
    channel_->disableAll();
    channel_->remove();
    int sockfd = channel_->getFd();
    loop_->queueInLoop(
        std::bind(&Connector::resetChannel, this)); // FIXME: unsafe
    return sockfd;
}

void Connector::resetChannel() { channel_.reset(); }
void Connector::setState(StateConn s) { state_ = s; }
void Connector::setNewConnectionCallback(const NewConnectionCallback &cb) { newConnectionCallback_ = cb; }
const InetAddress &Connector::getServerAddress() const { return serverAddr_; }