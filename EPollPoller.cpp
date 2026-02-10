#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <unistd.h>             // close
#include <cstring>              // memeset


// 自定义channel状态：
// channel未添加到epoll树上，也未添加到poller的map上
constexpr int kNew = -1;
// channel已添加到epoll树上，已添加到poller的map上
constexpr int kAdded = 1;
// channel从epoll树上删除，但还存在poller的map上
constexpr int kDeleted = 2;

/*
当一个进程调用exec()执行新程序时：
1. 进程的地址空间被新程序替换
2. 文件描述符默认会被子进程继承
3. 设置了FD_CLOEXEC的文件描述符会自动关闭
*/
EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop)
    , epollFd_(epoll_create1(EPOLL_CLOEXEC))
    , epollEvents_(kInitEventListSize)
{
    if (epollFd_ < 0)
    {
        LOG_FATAL("create_epoll1函数调用失败 error:%d\n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    close(epollFd_);
}

void EPollPoller::update(int operation, Channel* channel)
{
    epoll_event event;
    memset(&event, 0x00, sizeof(event));

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;

    // 关键操作：事件的数据指针指向channel指针
    event.data.ptr = channel;

    // 事件上epoll树或者更新epoll树上的事件
    if (epoll_ctl(epollFd_, operation, fd, &event) < 0)
    {
        // 上树操作发送错误
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("删除epoll树上的事件失败，errno：%d\n", errno);
        }
        else
        {
            LOG_FATAL("添加epoll树事件/修改epoll树事件失败：%d\n", errno);
        }
    }
}

void EPollPoller::updateChannel(Channel* channel)
{
    // 获取channel的状态
    const int index = channel->index();
    // 打印channel的日志信息
    LOG_INFO("func=%s => fd = %d events = %d, index=%d\n", __func__, channel->fd(), channel->events(), index);
    // channel不在epoll树上
    if (index == kNew || index == kDeleted)
    {
        // 是一个全新的channel，不在poller的map上
        if (index == kNew)
        {
            int fd = channel->fd();
            // 添加到poller的map上
            channels_[fd] = channel;
        }
        // 不是一个全新的channle，已经从epoll树上删除，但还存在poller的map上，重新上epoll树
        channel->set_index(kAdded);

        // 添加到epoll树上
        update(EPOLL_CTL_ADD, channel);
    }
    else
    // channel在epoll树上，对epoll树上的channel进行操作
    {
        // channel不监听任何事件，将channel从epoll树上删除
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            // channel的更新操作
            update(EPOLL_CTL_MOD, channel);
        }
    }
    
}

void EPollPoller::removeChannel(Channel* channel)
{    
    // 将channel从poller的map上删除
    int fd = channel->fd();
    channels_.erase(fd);
    // 日志打印
    LOG_INFO("func=%s, fd=%d\n", __func__, fd);
    // 将channel从epoll树上删除
    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);

}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    for (int i = 0; i < numEvents; i++)
    {
        Channel* channel = static_cast<Channel*>(epollEvents_[i].data.ptr);
        // 保存channel返回的事件类型
        channel->set_revents(channel->events());
        activeChannels->push_back(channel);
    }
    
}


Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    // __func__：C++11标准的函数名
    LOG_INFO("func=%s => fd total count:%lu \n", __func__, channels_.size());

    int numEvents = epoll_wait(epollFd_, &*epollEvents_.begin(), static_cast<int>(epollEvents_.size()), timeoutMs);
    int saveError = errno;
    Timestamp now(Timestamp::now());

    // 有监听事件响应，将响应的事件通过activeChannels传出
    if (numEvents > 0)
    {
        LOG_INFO("%d events happend \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == epollEvents_.size())
        {
            epollEvents_.resize(numEvents * 2);
        }
    }
    // 在timeoutMs秒内，没有监听的事件响应
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout \n", __func__);
    }
    // epoll_wait阻塞打断
    else
    {
        // 不是被系统信号中断的都要打印错误信息
        if (saveError != EINTR)
        {
            LOG_ERROR("EPollPoller::poll() error，errno=%d\n", saveError);
        }
    }

    return now;
}



