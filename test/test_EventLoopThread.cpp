// AI生成
#include "./../EventLoopThread.h"
#include "./../EventLoop.h"

#include <iostream>
#include <chrono>

void test_basic()
{
    std::cout << "=== 测试1: 基本功能 ===" << std::endl;
    
    EventLoopThread thread;
    EventLoop* loop = thread.startLoop();
    
    // 验证loop不为空
    if (loop == nullptr) {
        std::cout << "❌ 错误: loop为空!" << std::endl;
        return;
    }
    
    // 在主线程中通过runInLoop执行任务
    loop->runInLoop([]() {
        std::cout << "✅ EventLoop线程任务执行成功" << std::endl;
    });
    
    // 等待任务执行
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "✅ 基本功能测试通过" << std::endl;
}

void test_with_callback()
{
    std::cout << "\n=== 测试2: 带初始化回调 ===" << std::endl;
    
    bool callback_called = false;
    EventLoopThread::ThreadInitCallback cb = [&callback_called](EventLoop* loop) {
        std::cout << "✅ 初始化回调被调用，EventLoop地址: " << loop << std::endl;
        callback_called = true;
        
        // 可以在回调中做一些初始化工作
        loop->runInLoop([]() {
            std::cout << "    回调中设置的初始任务执行" << std::endl;
        });
    };
    
    EventLoopThread thread(cb, "TestThread");
    EventLoop* loop = thread.startLoop();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (!callback_called) {
        std::cout << "❌ 错误: 初始化回调未被调用!" << std::endl;
    } else {
        std::cout << "✅ 回调测试通过" << std::endl;
    }
}

#include <vector>
#include <thread>
#include <atomic>

void test_destruction()
{
    std::cout << "\n=== 测试4: 析构行为 ===" << std::endl;
    
    bool loop_quit_called = false;
    
    {
        // 创建一个EventLoop的mock用于测试
        class MockEventLoop : public EventLoop {
        public:
            bool& quit_flag;
            MockEventLoop(bool& flag) : quit_flag(flag) {}
            void quit() {
                quit_flag = true;
                EventLoop::quit();
                std::cout << "✅ quit()方法被调用" << std::endl;
            }
        };
        
        // 使用注入的方式测试（实际项目中可能需要修改设计）
        std::cout << "⚠️  析构测试需要特殊的mock设计" << std::endl;
    }
    
    // 简单版本：测试析构不崩溃
    try {
        EventLoopThread* thread = new EventLoopThread();
        thread->startLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        delete thread;  // 析构应该正常执行
        std::cout << "✅ 析构测试通过，无崩溃" << std::endl;
    } catch (...) {
        std::cout << "❌ 析构过程中出现异常!" << std::endl;
    }
}

void test_edge_cases()
{
    std::cout << "\n=== 测试5: 边缘情况 ===" << std::endl;
    
    // 测试1: 空回调
    {
        EventLoopThread thread(nullptr, "EmptyCallbackThread");
        EventLoop* loop = thread.startLoop();
        if (loop != nullptr) {
            std::cout << "✅ 空回调测试通过" << std::endl;
        }
    }

}

#include <chrono>

void test_performance()
{
    std::cout << "\n=== 测试6: 性能测试 ===" << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    const int THREAD_COUNT = 10;
    std::vector<std::unique_ptr<EventLoopThread>> threads;
    std::vector<EventLoop*> loops;
    
    // 创建多个EventLoopThread
    for (int i = 0; i < THREAD_COUNT; ++i) {
        auto thread = std::make_unique<EventLoopThread>(
            nullptr, "PerfThread_" + std::to_string(i)
        );
        loops.push_back(thread->startLoop());
        threads.push_back(std::move(thread));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "创建 " << THREAD_COUNT << " 个EventLoopThread耗时: " 
              << duration.count() << "ms" << std::endl;
    
    // 验证所有loop都成功创建
    bool all_valid = true;
    for (auto loop : loops) {
        if (loop == nullptr) {
            all_valid = false;
            break;
        }
    }
    
    if (all_valid) {
        std::cout << "✅ 性能测试通过" << std::endl;
    }
    
    // 清理
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}


int main()
{
    std::cout << "开始测试 EventLoopThread 类..." << std::endl;
    
    try {
        test_basic();
        test_with_callback();
        test_destruction();
        test_edge_cases();
        test_performance();
        
        std::cout << "\n🎉 所有测试完成!" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "\n❌ 测试过程中出现异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}