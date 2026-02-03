#pragma once
#include "noncopyable.h"
#include "nonmoveable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>
#include <memory>


class Channel;
class EventLoop;


class Poller : private noncopyable, private nonmoveable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    // 判断channel是否在channels_中
    bool hasChannel(Channel* channel) const;

    // 提供获取具体实现的IO复用机制
    static std::unique_ptr<Poller> newDefaultPoller(EventLoop* loop);
protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    // 持有channel对象，但不对channel的生命周期进行管理
    ChannelMap channels_;
private:
    // 线程同步的事情已经交给EventLoop了，在Poller或者Poller的派生类这一层完全不需要使用到EventLoop，这里为什么要持有EventLoop有点不理
    // 解？为了组件的一致性所以即使不需要也持有？即这个持有动作是为了表明一个IO复用机制只能绑定一个事件循环，而一个事件循环只能绑定一个线程
    EventLoop* ownerLoop_;
};