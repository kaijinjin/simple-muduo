#pragma once
#include "noncopyable.h"
#include "nonmoveable.h"
#include "Timestamp.h"

#include <functional>               // function
#include <memory>                   // shared_ptr、weak_ptr
#include <sys/epoll.h>              // EPOLLIN、EPOLLPRI、EPOLLOUT


// 向前声明不引入头文件-编译防火墙：减少头文件依赖，加快编译
class EventLoop;


class Channel : private noncopyable, private nonmoveable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel() = default;

    void handleEvent(Timestamp receiveTime);
    
    // 设置回调
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    void tie(const std::shared_ptr<void>&);
    // 返回我们需要监听的文件描述符
    int fd() const { return fd_; }
    // 返回我们设置的需要监听的事件
    int events() const { return events_; }
    // 保存发生的事件
    void set_revents(int revents) { revents_ = revents; }

    // 设置fd相应的事件状态：监听读、写，不监听读、写、不监听任何事件
    // 这些函数确实经常被调用，虽然update的调用无法被内联优化，但是本身的函数调可以内联，从2次函数调用变成1次
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void diaableAll() { events_ = kNoneEvent; update(); }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    EventLoop* owerLoop() { return loop_; }

    // 将channel从事件循环中移除不在监听
    void remove();

private:

    // 对socketFd的事件进行更新操作
    void update();
    // 通过线程安全的方式来执行回调函数
    void handleEventWithGuard(Timestamp receiveTime);

    // C++17 constexpr编译期常量
    static constexpr int kNoneEvent = 0;
    /*
    EPOLLIN：读事件信号
    EPOLLPRI：带外数据MSG_OOB标志，如send(sockfd, "urgent", 6, MSG_OOB);
    接收端通过 EPOLLPRI 接收，数据会"插队"到接收队列前面，
    但OOB实际只能携带一个字节的数据不够现代应用使用，并且由于各个TCP协议实现不一样（linux、windows、bsd），
    现代应用已经很少使用EPOLLPRI，kReadEvent带上这个标志是为了兼容或者历史习惯
    现代应用可以通过应用层使用带内数据标志来实现数据的优先级：
    // 使用消息头标识优先级
    struct Message 
    {
        uint8_t priority;  // 0=normal, 1=urgent, 2=critical
        uint32_t length;
        char data[];
    };
    应用层处理，比 TCP OOB 更可靠
    */
    static constexpr int kReadEvent = EPOLLIN | EPOLLPRI;
    static constexpr int kWriteEvent = EPOLLOUT;

    // 事件循环
    EventLoop* loop_;
    // 文件描述符
    const int fd_;
    // socketFd监听的事件类型
    int events_;
    // IO复用返回的socketFd响应的事件类型
    int revents_;
    // channle的状态：是否在epoll树上
    int index_;

    // 防止在链接已经被关闭的情况下还去操作链接
    std::weak_ptr<void> tie_;
    bool tied_;

    // 持有的回调函数
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};