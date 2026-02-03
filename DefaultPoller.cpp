#include "Poller.h"
#include "EPollPoller.h"

#include <memory>               // unique_ptr


std::unique_ptr<Poller> Poller::newDefaultPoller(EventLoop* loop)
{
    if (getenv("MUDUO_USE_POLL"))
    {
        return nullptr;
    }
    else
    {
        // return new EPollPoller(loop);
        return std::make_unique<EPollPoller>(loop);
    }
}