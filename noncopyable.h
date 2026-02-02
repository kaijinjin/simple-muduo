#pragma once


/*
将构造、析构设置为保护：派生类能正常构造对象与析构对象
将拷贝构造、赋值运算符删除掉：派生类在拷贝构造与赋值运算时会调用基类的拷贝构造与赋值运算来完成基类属性初始化，但由于被基类的拷贝构造与
赋值运算符被删除了，所以会直接报错
*/

class noncopyable
{
protected:
    noncopyable() = default;
    ~noncopyable() = default;
    
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
};