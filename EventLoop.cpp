#include "Logger.h"
#include "EventLoop.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>

const int kPollTimeMs = 10000;

// 防止一个线程创建多个EventLoop
thread_local EventLoop* t_loopInThisThread = nullptr;


int createEventfd()
{
    // EFD_NONBLOCK 文件描述符设置为非阻塞
    // EFD_CLOEXEC 调用exec执行新程序时，在新程序关闭这个文件描述符
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;

}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(std::make_unique<Channel>(this, wakeupFd_))
    , callingPendingFunctors_(false)
{
    LOG_INFO("事件循环：%p创建在线程：%d\n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("一个线程：%p创建了两个事件循环：%d， %d\n", threadId_, this, t_loopInThisThread);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置wakeupfd的channel的读回调函数 
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 设置wakeupfd的channel对读事件进行监听
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    /*
    为什么要先disableAll在remove？
    1. 某个channel的数据包到达网卡，TCP协议栈处理，设置socket为可读
    2. 某个channel事件被加入epoll的就绪队列               ← 这一步在内核中
    3. 应用程序调用 channel->remove()删除某个channel      ← 这一步在应用层
    4. epoll_ctl(EPOLL_CTL_DEL) 从监听树移除某个channel
    5. 应用程序调用 epoll_wait()
    6. epoll_wait() 返回就绪事件             ← 包括第2步加入的事件！
    7. epoll_wait中返回了已经被删除的channel，对这个channel进行操作的行为是未定义的，可能导致崩溃
    */

    wakeupChannel_->disableAll();
    // 从poller的map上删除，如果还未从epoll树上删除就执行删除操作，并且将channle状态设置成kNew
    wakeupChannel_->remove();
    close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("事件循环：%p开启循环\n", this);

    while(!quit_)
    {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        // 执行clientfd相关回调
        for (Channel* channel : activeChannels_)
        {
            channel->handleEvent(pollReturnTime_);
        }

        // 执行loop之间设置的回调操作
        doPendingFunctors();
    }

    LOG_INFO("事件循环%p结束\n", this);
    looping_ = false;

}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    // 执行回调的EventLoop通过临时变量直接交换任务队列中的任务，快速释放锁
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    // 执行回调
    for (const Functor& functor : functors)
    {
        functor();
    }

    callingPendingFunctors_ = false;
}

void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::wakeup() write %lu bytes instead of 8", n);
    }
}


void EventLoop::quit()
{
    quit_ = true;
    // 如果是线程1调用的线程2的EventLoop退出，线程2有可能阻塞在poller.poll，直接唤醒结束循环
    if (!isInLoopThread())
    {
        // 向线程2的wakeupfd写一个消息，去唤醒可能阻塞在poller.poll处的线程2
        wakeup();
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    // 将cb回调放入回调任务队列
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    // 唤醒条件1：eventLoop不在自己的线程， 唤醒条件2：eventloop正在执行回调，为了上面提交的回调任务被及时执行就让这个EventLoop执行完成后再去任务队列中去新添加的回调
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
    {
        cb();
    }
    else
    {
        queueInLoop(cb);
    }
}

void EventLoop::removeChannel(Channel* channle)
{
    poller_->removeChannel(channle);
}

void EventLoop::updateChannel(Channel* channle)
{
    poller_->updateChannel(channle);
}

bool EventLoop::hasChannel(Channel* channle)
{
    return poller_->hasChannel(channle);
}