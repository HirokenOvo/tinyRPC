#include "Socket.h"
#include "InetAddress.h"
#include "Logger.h"

#include <netinet/tcp.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>

Socket::Socket(int sockfd)
    : sockfd_{sockfd}
{
}

Socket ::~Socket() { close(sockfd_); }

int Socket::getFd() const { return sockfd_; }

void Socket::bindAddress(const InetAddress &localaddr)
{
    if (bind(sockfd_, (sockaddr *)localaddr.getSockAddr(), sizeof(sockaddr_in)))
        LOG_FATAL("bind sockfd:%d fail \n", sockfd_);
}

void Socket::listen()
{
    if (::listen(sockfd_, 1024))
        LOG_FATAL("listen sockfd:%d fail \n", sockfd_);
}

int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in addr;
    socklen_t addrlen = sizeof addr;
    bzero(&addr, sizeof addr);
    // SOCK_NONBLOCK 在调用I/O操作时,若操作无法立即完成,不会阻塞进程,而是返回一个错误码
    int connfd = ::accept4(sockfd_, (sockaddr *)&addr, &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0)
        peeraddr->setSockAddr(addr);

    return connfd;
}

void Socket::shutdownWrite()
{
    if (::shutdown(sockfd_, SHUT_WR) < 0)
        LOG_ERROR("shutdownWrite error");
}

void Socket::setTcpNoDelay(bool on)
{
    int opt = on;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof opt);
}

void Socket::setReuseAddr(bool on)
{
    int opt = on;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
}
void Socket::setReusePort(bool on)
{
    int opt = on;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof opt);
}
void Socket::setKeepAlive(bool on)
{
    int opt = on;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof opt);
}