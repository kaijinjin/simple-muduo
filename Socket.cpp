#include "Socket.h"
#include "InetAddress.h"
#include "Logger.h"

#include <unistd.h>                 // close
#include <sys/socket.h>             // bind、listen、accept、shutdown
#include <netinet/tcp.h>            // TCP_NODELAY
#include <cstring>                  // memset



Socket::~Socket()
{
    close(sockfd_);
}

void Socket::bindAddress(const InetAddress& localaddr)
{
    if (0 != bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("绑定 sockfd：%d失败，errno：%d\n", sockfd_, errno);
    }
}

void Socket::listenFd()
{
    /*
    1024=未完成链接队列值+已完成队列的最大值
    未完成链接-未完成3次握手：客户端发送了SYN，服务端返回了SYN+ACK，服务端等待客户端的ACK
    已完成链接-已完成3次握手：等待服务端accept
    */
    if (0 != listen(sockfd_, 1024))
    {
        LOG_FATAL("监听sockfd：%d失败\n", sockfd_);
    }
}

int Socket::acceptFd(InetAddress* peeraddr)
{
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    memset(&addr, 0x00, len);
    // SOCK_NONBLOCK：设置通信文件描述符为非阻塞
    // SOCK_CLOEXEC：如果使用了exec就在新进程中关闭这个通信文件描述符
    int connfd = accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0)
    {
        peeraddr->setSockAddr(addr);
    }

    return connfd;
}

void Socket::shutdownWrite()
{
    if (shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR("关闭写失败");
    }
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}