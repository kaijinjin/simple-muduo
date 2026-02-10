#include "TcpServer.h"

#include <functional>
#include <string>
#include <cstring>

#include "Accept.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Logger.h"
#include "TcpConnection.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"

static EventLoop *CheckEventLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __func__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *eventLoop,
                     const InetAddress &listenAddr,
                     const std::string &name,
                     Option option)
    : eventLoop_(eventLoop)
    , ipPort_(listenAddr.toIpPort())
    , name_(name)
    , acceptor_(std::make_unique<Acceptor>(eventLoop_, listenAddr, option))
    , threadPool_(std::make_unique<EventLoopThreadPool>(eventLoop_, name))
    , connectionCallback_()
    , messageCallback_()
    , writeCompleteCallback_()
    , started_(false)
    , nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connectionMap_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
        /*
        为什么不直接使用item.second？bind是复制参数，不会导致引用计数为零
        item.second->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, item.second));
        item.second.reset();
        这种写法意图不够清晰：源码写法清晰：
        1、接管所有权
        2、清理容器
        3、传递所有权
        为什么不直接erase？为了遍历安全，erase会导致迭代器失效，每次需要手动保存迭代器，而且多次erase可能导致rehash
        */
    }
    
}

void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    // 防御性编程：防止一个TcpServer被start多次
    if (started_++ == 0)
    {
        threadPool_->start(threadInitCallback_);
        eventLoop_->runInLoop(std::bind(&Acceptor::listenFd, acceptor_.get()));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // 获取subLoop
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - 新链接[%s] from %s \n", name_.c_str(), ipPort_.c_str(),peerAddr.toIpPort().c_str());

    sockaddr_in local;
    memset(&local, 0x00, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if (getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("TcpServer::newConnection::getsockname错误，errno:%d\n", errno);
    }
    InetAddress localAddress(local);
    // 为什么要每次去获取？因为设置监听ipport的时候，可能监听多个ipport还有可能随机监听，所以ipport每次都去动态的获取
    TcpConnectionPtr conn(std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddress, peerAddr));

    connectionMap_[connName] = conn;
    // 默认什么都不做
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));

}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    // 在baseLoop中执行
    eventLoop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    // 在baseLoop中执行
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connnection%s\n", name_.c_str(), conn->name().c_str());
    connectionMap_.erase(conn->name());
    // 到ioLoop中执行
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));

}