#pragma once

#include "noncopyable.h"
#include "nonmoveable.h"

#include <functional>
#include <string>
#include <memory>               // shared_ptr
#include <thread>
#include <atomic>


class Thread : private noncopyable, private nonmoveable
{
public:
    using ThreadFunc = std::function<void()>;
    
    explicit Thread(ThreadFunc, const std::string name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string& name() const { return name_; }

    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    inline static std::atomic_int numCreated_{0};
};