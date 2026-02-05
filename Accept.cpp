#include "Accept.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/socket.h>
#include <unistd.h>                 // close

static int createNonblocking()
{
    // SOCK_STREAM：使用TCP协议
    int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d 创建监听套接字失败，errno=%d\n", __FILE__, __func__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : eventLoop_(loop)
    , acceptSocket_(createNonblocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr (true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listenFd()
{
    listenning_ = true;
    acceptSocket_.listenFd();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead()
{
    InetAddress peeraddr;
    int connfd = acceptSocket_.acceptFd(&peeraddr);
    if (connfd > 0)
    {
        if (newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peeraddr);
        }
        else
        {
            close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept发生错误：errno:%d\n", __FILE__, __func__, __LINE__, errno);
        if (errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d 文件描述符达到上限 \n", __FILE__, __func__, __LINE__);
        }
    }


}