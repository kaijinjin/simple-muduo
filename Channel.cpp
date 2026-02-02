#include "Channel.h"
#include "Logger.h"
#include "EventLoop.h"


Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(0)
    , tied_(false)
{

}

void Channel::handleEvent(Timestamp receiveTime)
{
    // tied_==true：说明这是一个tcp链接，需要检查一下tcp是否已经被销毁，没被销毁才去执行回调函数
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    // tied_==false：说明不是一个tcp链接，不必在意生命周期的问题，直接调用回调函数
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

// tcp链接在初始化的时候调用这个函数，只会调用一次没必要放头文件
void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;
    tied_ = true;
}

// 虽然简单放在头文件中可能内联坚守一次函数调用？但是如果放在头文件中实现需要在头文件中引入EventLoop，导致头文件依赖与编译变慢
void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::update()
{
    loop_->updateChannel(this);
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("执行channel的回调，revents：%d\n", revents_);
    /*
    当对端close()时：
    对端：close(sockfd) → 发送 FIN 报文
    本端：内核收到 FIN → 
    1. 将"连接关闭"事件放入接收队列（作为一个特殊"数据"）
    2. 更新连接状态为"半关闭"
    3. epoll_wait() 返回：
        - EPOLLIN：有"数据"可读（这个"数据"就是FIN到达事件）
        - EPOLLHUP：连接状态变为半关闭
    一个事件，两个标志位
    遵循现读取FIN报文，再进行关闭链接的操作
    第一次事件循环：revents = EPOLLIN | EPOLLHUP，所以先去读FIN报文，muduo是LT模式，EPOLLHUB没有处理还会继续通知
    第二次事件循环：revents = EPOLLHUP 进行链接的关闭操作
    */
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_) closeCallback_();
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_) errorCallback_();
    }

    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_) readCallback_(receiveTime);
    }

    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_) writeCallback_();
    }
}