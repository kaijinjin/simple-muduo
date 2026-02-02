#pragma once


/*
仿照noncopyable类的禁止移动的nonmoveable类，继承这个类的派生类都不能进行移动、移动运算符
*/

class nonmoveable
{
protected:
    nonmoveable() = default;
    ~nonmoveable() = default;

public:
    nonmoveable(nonmoveable&&) = delete;
    nonmoveable& operator=(nonmoveable&&) = delete;
};