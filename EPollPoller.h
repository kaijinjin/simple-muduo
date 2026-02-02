#pragma once
#include "Poller.h"

#include <sys/epoll.h>
#include <vector>


class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    // 更新channel
    void updateChannel(Channel* channel) override;
    // 将channle从poller的map上删除、将channel从epoll树上删除
    void removeChannel(Channel* channel) override;
private:
    // 填写活跃的链接
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    // 更新channel：channel上epoll树操作、修改epoll树事件（channle）操作的封装
    void update(int operation, Channel* channel);
    // 初始Epoll大小是16
    static constexpr int kInitEventListSize = 16;
    // epoll树根节点
    int epollFd_;

    using EventList = std::vector<epoll_event>;
    // epoll事件集合
    EventList epollEvents_;
};