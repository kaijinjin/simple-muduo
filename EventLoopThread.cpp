#include "EventLoopThread.h"
#include "EventLoop.h"


EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const std::string& name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , callback_(cb) {}


EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        // 让子线程退出循环
        loop_->quit();
        // 等待子线程退出循环
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    // 启动子线程，子线程运行EventLoopThread::threadFunc()
    thread_.start();
    EventLoop* loop = nullptr;
    sleep(1);
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 等待子线程赋值后通知
        while (loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    
    return loop;
}


void EventLoopThread::threadFunc()
{
    // 子线程创建EventLoop
    EventLoop loop;
    
    // callback_如果存在，就对EventLoop开始循环前做一些准备工作
    if(callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
