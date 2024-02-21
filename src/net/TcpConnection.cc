#include "include/Channel.h"
#include "include/EventLoop.h"
#include "include/Logger.h"
#include "include/Socket.h"
#include "include/TcpConnection.h"

#include <unistd.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &namePre,
                             int sockfd,
                             const InetAddress localAddr,
                             const InetAddress peerAddr)
    : loop_{CheckLoopNotNull(loop)}, name_{namePre}, state_{StateConn::kConnecting}, reading_{true},
      socket_{std::make_unique<Socket>(sockfd)}, channel_{std::make_unique<Channel>(loop, sockfd)},
      localAddr_{localAddr}, peerAddr_{peerAddr},
      highWaterMark_{64 * 1024 * 1024} // 64M
{
    // poller给channel通知感兴趣的事件发生了,channel会回调相应的操作函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::creator[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}
TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::destroyor[%s] at fd=%d state=%d \n",
             name_.c_str(), channel_->getFd(), (int)state_);
}

// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // 向poller注册channel的epollin事件

    connectionCallback_(shared_from_this()); // 新连接建立,执行回调
}
// 发送数据
void TcpConnection::send(const std::string &buf)
{
    if (isConnected())
    {
        auto func{std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size())};
        loop_->runInLoop(func);
    }
}
// 应用写的快 但内核发送数据慢 需把待发送数据写入缓冲区 设置了水位回调
void TcpConnection::sendInLoop(const void *message, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }
    // channel_第一次开始写数据,且缓冲区没有待发送数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->getFd(), message, len);
        if (nwrote >= 0)
        {
            remaining -= nwrote;
            if (!remaining && writeCompleteCallback_)
            {
                // 数据全部发送完成,直接写完成回调
                //???为什么是queueInLoop而不用runInLoop
                //!!!https://cstardust.github.io/2022/10/12/muduo-%E5%85%B3%E4%BA%8EEventLoop%E7%9A%84runInLoop%E4%B8%8EqueueInLoop/
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote<0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE  RESET
                {
                    faultError = true;
                }
            }
        }
    }

    /**
     * 说明当前这一次write并没有把数据全部发送出去,剩余的数据需要保存到缓冲区当中并给channel注册epollout事件
     * poller发现tcp的发送缓冲区有空间,会通知相应的sock-channel调用writeCallback_回调方法
     * 也就是调用TcpConnection::handleWrite方法,把发送缓冲区中的数据全部发送完成
     */
    if (!faultError && remaining > 0)
    {
        // 目前发送缓冲区中待发送数据的长度
        size_t oldLen = outputBuffer_.readableBytes();
        // 若缓冲区加上当前剩余未发送的数据超过标记线,执行高水位回调
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));

        outputBuffer_.append((char *)message + nwrote, remaining);
        if (!channel_->isWriting())
            channel_->enableWriting(); // 如果不注册channel的写事件,poller不会给channel通知epollout
    }
}

// 关闭连接
void TcpConnection::shutdown()
{
    if (isConnected())
    {
        setState(kDisconnecting);
        auto func{std::bind(&TcpConnection::shutdownInLoop, this)};
        loop_->runInLoop(func);
    }
}
void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting())   // 说明outputBuffer中的数据已经全部发送完毕
        socket_->shutdownWrite(); // 关闭写端通知对方没有数据要发送,仍可读取对方数据,处于半关闭连接状态
}

// 连接销毁
void TcpConnection::connectDestroyed()
{
    if (isConnected())
    {
        setState(StateConn::kDisconnected);
        channel_->disableAll(); // 将channel所有感兴趣的事件从poller中del掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // 把channel从poller中删除掉
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->getFd(), &savedErrno);
    if (n > 0)
        // 有可读事件发生,调用用户传入的回调操作
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    else if (!n)
        handleClose();
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    // 写入事件判断套接字是否处于写入状态，以避免不必要的写入操作导致的问题，
    // TCP套接字在连接建立后，通常会一直处于可读状态,因此不需要额外的判断
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->getFd(), &savedErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));

                if (state_ == kDisconnecting)
                    shutdownInLoop();
            }
        }
        else
            LOG_ERROR("TcpConnection::handleWrite");
    }
    else
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->getFd());
}
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->getFd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    connectionCallback_(shared_from_this());
    closeCallback_(shared_from_this()); // 执行的是TcpServer::removeConnection回调方法
}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->getFd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
        err = errno;
    else
        err = optval;
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}

EventLoop *TcpConnection::getLoop() const { return loop_; }
const std::string TcpConnection::getName() const { return name_; }
const InetAddress TcpConnection::getLocalAddress() const { return localAddr_; }
const InetAddress TcpConnection::getPeerAddress() const { return peerAddr_; }

bool TcpConnection::isConnected() const { return state_ == StateConn::kConnected; }

void TcpConnection::setState(const StateConn state) { state_ = state; }
void TcpConnection::setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
void TcpConnection::setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
void TcpConnection::setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
void TcpConnection::setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }
void TcpConnection::setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
{
    highWaterMarkCallback_ = cb;
    highWaterMark_ = highWaterMark;
}