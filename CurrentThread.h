#pragma once

#include <unistd.h>
#include <sys/syscall.h>


namespace CurrentThread
{
    // thread_local C++11引入：每个线程对该变量有独立的副本
    // inline变量：C++17引入，允许多个定义（值必须相同），在链接时合并
    inline thread_local int t_cacheTid = 0;

    inline void cacheTid()
    {
        if (t_cacheTid == 0)
        {
            t_cacheTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }

    inline int tid()
    {
        // __builtin_expect分支预测：(t_cacheTid == 0, 0)：告诉编译器，我期望 t_cachedTid == 0 这个条件很少成立
        if (__builtin_expect(t_cacheTid == 0, 0))
        {
            cacheTid();
        }
        return t_cacheTid;
    }

}