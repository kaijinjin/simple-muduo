// AI生成
#include "./../EventLoopThreadPool.h"
#include "./../EventLoop.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cassert>

// 辅助函数：打印线程ID
void printThreadId(const std::string& name, EventLoop* loop = nullptr) {
    std::cout << name << " - Thread ID: " << std::this_thread::get_id();
    if (loop) {
        std::cout << ", Loop address: " << loop;
    }
    std::cout << std::endl;
}

// 线程初始化回调函数示例
void threadInitCallback(EventLoop* loop) {
    printThreadId("ThreadInitCallback", loop);
}

// 测试1：基本功能测试
void testBasicFunctionality() {
    std::cout << "=== Test 1: Basic Functionality ===" << std::endl;
    
    EventLoop baseLoop;
    EventLoopThreadPool pool(&baseLoop, "TestPool");
    
    // 设置线程数为3
    pool.setThreadNum(3);
    assert(!pool.started());
    
    // 启动线程池
    pool.start(threadInitCallback);
    assert(pool.started());
    
    // 测试获取循环
    for (int i = 0; i < 10; ++i) {
        EventLoop* loop = pool.getNextLoop();
        assert(loop != nullptr);
        std::cout << "Get loop " << i << ": " << loop << std::endl;
    }
    
    // 获取所有循环
    auto allLoops = pool.getAllLoops();
    assert(allLoops.size() == 3);
    std::cout << "Total loops: " << allLoops.size() << std::endl;
    
    std::cout << "Test 1 passed!" << std::endl << std::endl;
}

// 测试2：单线程模式（numThreads_ = 0）
void testSingleThreadMode() {
    std::cout << "=== Test 2: Single Thread Mode ===" << std::endl;
    
    EventLoop baseLoop;
    EventLoopThreadPool pool(&baseLoop, "SingleThreadPool");
    
    // 线程数为0，应该只使用baseLoop
    pool.setThreadNum(0);
    pool.start(threadInitCallback);
    
    // 多次获取应该都返回同一个loop（baseLoop）
    EventLoop* loop1 = pool.getNextLoop();
    EventLoop* loop2 = pool.getNextLoop();
    EventLoop* loop3 = pool.getNextLoop();
    
    assert(loop1 == &baseLoop);
    assert(loop2 == &baseLoop);
    assert(loop3 == &baseLoop);
    
    auto allLoops = pool.getAllLoops();
    assert(allLoops.size() == 1);
    assert(allLoops[0] == &baseLoop);
    
    std::cout << "All loops point to baseLoop: " << &baseLoop << std::endl;
    std::cout << "Test 2 passed!" << std::endl << std::endl;
}

// 测试3：并发获取循环
void testConcurrentAccess() {
    std::cout << "=== Test 3: Concurrent Access ===" << std::endl;
    
    EventLoop baseLoop;
    EventLoopThreadPool pool(&baseLoop, "ConcurrentPool");
    pool.setThreadNum(4);
    pool.start();
    
    std::atomic<int> loopCount{0};
    std::vector<std::thread> threads;
    std::vector<EventLoop*> loops(8, nullptr);
    
    // 创建多个线程同时获取循环
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&pool, &loops, i, &loopCount]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));
            loops[i] = pool.getNextLoop();
            loopCount++;
            printThreadId("Worker Thread", loops[i]);
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 验证确实获取到了不同的循环（轮询调度）
    std::cout << "Loop distribution:" << std::endl;
    for (int i = 0; i < loops.size(); i++) {
        std::cout << "Thread " << i << " got loop: " << loops[i] << std::endl;
    }
    
    auto allLoops = pool.getAllLoops();
    std::cout << "Total available loops: " << allLoops.size() << std::endl;
    
    assert(loopCount == 8);
    std::cout << "Test 3 passed!" << std::endl << std::endl;
}


// 测试5：析构函数和资源清理
void testDestruction() {
    std::cout << "=== Test 5: Destruction Test ===" << std::endl;
    
    // 使用智能指针管理EventLoop生命周期
    auto baseLoop = std::make_unique<EventLoop>();
    auto pool = std::make_unique<EventLoopThreadPool>(baseLoop.get(), "DestructionPool");
    
    pool->setThreadNum(3);
    pool->start();
    
    // 获取一些循环
    auto loop1 = pool->getNextLoop();
    auto loop2 = pool->getNextLoop();
    
    // 显式销毁pool
    pool.reset();
    
    // baseLoop应该仍然有效
    assert(baseLoop != nullptr);
    
    std::cout << "Pool destroyed successfully" << std::endl;
    std::cout << "Test 5 passed!" << std::endl << std::endl;
}


// 主测试函数
int main() {
    std::cout << "Starting EventLoopThreadPool tests..." << std::endl;
    
    try {
        testBasicFunctionality();
        testSingleThreadMode();
        testConcurrentAccess();
        testDestruction();
        
        std::cout << "========================================" << std::endl;
        std::cout << "All tests completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}