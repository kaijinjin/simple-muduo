#pragma once
#include "noncopyable.h"
#include "nonmoveable.h"
#include "Callbacks.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>

class EventLoop;
class InetAddress;
class Acceptor;
class EventLoopThreadPool;


class TcpServer : private noncopyable, private nonmoveable
{
public:
    // 线程初始化回调函数类型
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop* loop, 
              const InetAddress& listenAddr,
              const std::string& nameArg,
              Option option = kReusePort);
    ~TcpServer();
    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

    void setThreadNum(int numThreads);

    void start();
private:
    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop* eventLoop_;
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;

    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;   

    ThreadInitCallback threadInitCallback_;

    std::atomic_int started_;

    int64_t nextConnId_;
    ConnectionMap connectionMap_;
    
};  