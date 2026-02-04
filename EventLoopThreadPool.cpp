#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"


EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& name)
    : baseLoop_(baseLoop)
    , name_(name)
    , started_(false)
    , numThreads_(0)
    , next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool(){}

void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        eventLoopThreads_.push_back(std::make_unique<EventLoopThread>());
        eventLoops_.push_back(eventLoopThreads_[i]->startLoop());
    }

    if (numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
    }
    
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop = baseLoop_;

    if (!eventLoops_.empty())
    {
        loop = eventLoops_[next_++];
        if (next_ >= static_cast<int>(eventLoops_.size()))
        {
            next_ = 0;
        }
    }

    return loop;
}


std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if (eventLoops_.empty())
    {
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else
    {
        return eventLoops_;
    }
}