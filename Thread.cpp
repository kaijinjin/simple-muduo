#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>              // sem_t


void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}

Thread::Thread(ThreadFunc func, const std::string name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach();
    }
}

void Thread::start()
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        // 获取线程tid
        tid_ = CurrentThread::tid();
        // 子线程完成初始化操作，通知主线程
        sem_post(&sem);
        // 子线程调用线程函数
        func_();
    }));

    // 主线程在这里等待子线程初始化完成后再继续执行
    sem_wait(&sem);
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}
