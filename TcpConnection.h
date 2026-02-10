#pragma once
#include "noncopyable.h"
#include "nonmoveable.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Buffer.h"

#include <atomic>
#include <string>

class EventLoop;
class Socket;
class Channel;

// std::enable_shared_from_this 是一个模板基类，允许一个对象安全地获取指向自身的 shared_ptr，即使该对象已被 shared_ptr 管理。
class TcpConnection : private noncopyable, private nonmoveable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* eventLoop, 
                const std::string name, 
                int sockfd, 
                const InetAddress& localAddr, 
                const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return eventLoop_; }
    const std::string& name() const { return name_; }
    const InetAddress& getLocalAddress() const { return localAddr_; }
    const InetAddress& getPeerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == StateE::kConnected; }

    void send(const std::string& buf);
    // 半关闭：关闭写端
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb) { highWaterMarkCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }

    // 链接建立-在服务端accept后
    void connectEstablished();
    // 
    void connectDestroyed();

private:
    enum class StateE : uint8_t { kDisconnected, kConnecting, kConnected, kDisconnecting };
    void setState(StateE state) { state_ = state; }

    void handleRead(Timestamp );
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const char* message, size_t len);
    void shutdownInLoop();

    EventLoop* eventLoop_;
    const std::string name_;
    // C++17枚举类型原子操作
    std::atomic<StateE> state_;
    // ?
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    /*
    定义：R = 可能重入的函数
    writeCompleteCallback_ ∈ R  用户可能在回调中 send()
    connectionCallback_ ∉ R     用户不太可能在回调中 accept() 新连接
    closeCallback_ ∉ R          连接已关闭，不能再操作
    所以：
    对 R 类回调，需要 queueInLoop 避免重入
    对 ∉ R 类回调，可以直接调用
    */
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;
};