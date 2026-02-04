#pragma once

#include "noncopyable.h"
#include "nonmoveable.h"

class InetAddress;

class Socket : private noncopyable, private nonmoveable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd) {}

    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress& localaddr);
    void listenFd();
    int acceptFd(InetAddress* peeraddr);

    // 半关闭-关闭写
    void shutdownWrite();
    // 禁用nagle算法，nagle算法：收到小的数据包先进行缓冲存储，积累到一定大小才发送，这样能减少频繁发送数据包，但是会增加延迟
    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

private:
    const int sockfd_;

};

