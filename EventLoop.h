#pragma once
#include "noncopyable.h"
#include "nonmoveable.h"
#include "CurrentThread.h"
#include "Timestamp.h"

#include <memory>               // unique_ptr
#include <atomic>
#include <mutex>
#include <functional>
#include <vector>

class Channel;
class Poller;


class EventLoop : private noncopyable, private nonmoveable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 在当前loop中执行cb
    void runInLoop(Functor cb);
    // 将cb放入队列，唤醒Loop所在的线程去执行cb
    void queueInLoop(Functor cb);

    // 唤醒Loop所在的线程
    void wakeup();

    void removeChannel(Channel* channel);
    void updateChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断EventLoop对象是否在自己的线程里面（一个线程一个EventLoop），但是主Loop能够持有IOLoop对象
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
private:
    void handleRead();
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;
    std::atomic_bool quit_;

    const pid_t threadId_;

    // poller返回监听事件的时间点
    Timestamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    // 标识当前loop是否有需要执行的回调操作
    std::atomic_bool callingPendingFunctors_;
    // 存储当前loop需要执行的回调操作
    std::vector<Functor> pendingFunctors_;
    // 保护pendingFunctors_的线程安全
    std::mutex mutex_;
};