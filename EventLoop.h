#pragma once
#include "noncopyable.h"
#include "nonmoveable.h"


class Channel;


class EventLoop : private noncopyable, private nonmoveable
{
public:
    void removeChannel(Channel* channel);
    void updateChannel(Channel* channel);
private:

};