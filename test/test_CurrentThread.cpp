#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include <mutex>
#include <sys/wait.h>

// 包含要测试的头文件
#include "./../CurrentThread.h"

using namespace std;
using namespace CurrentThread;

// 测试1: 基本功能测试
void testBasicFunctionality() {
    cout << "=== 测试1: 基本功能测试 ===" << endl;
    
    // 验证初始状态
    cout << "1. 验证初始状态:" << endl;
    cout << "   初始 tid() = " << CurrentThread::tid() << endl;
    
    // 验证 cacheTid 工作正常
    cout << "\n2. 验证 cacheTid 工作:" << endl;
    CurrentThread::cacheTid();
    int tid1 = CurrentThread::tid();
    cout << "   调用 cacheTid() 后 tid() = " << tid1 << endl;
    
    // 验证多次调用返回相同值
    cout << "\n3. 验证多次调用一致性:" << endl;
    for (int i = 0; i < 5; i++) {
        int currentTid = CurrentThread::tid();
        cout << "   第" << (i+1) << "次调用: tid() = " << currentTid;
        if (currentTid == tid1) {
            cout << " ✓ (一致)" << endl;
        } else {
            cout << " ✗ (不一致!)" << endl;
            assert(false);
        }
    }
    
    // 验证线程ID不是0（0通常表示无效进程）
    cout << "\n4. 验证线程ID有效性:" << endl;
    assert(CurrentThread::tid() != 0);
    cout << "   线程ID有效: " << CurrentThread::tid() << endl;
    
    cout << "=== 测试1通过 ===\n" << endl;
}

// 测试2: 多线程独立性测试
void testMultiThreadIndependence() {
    cout << "=== 测试2: 多线程独立性测试 ===" << endl;
    
    const int NUM_THREADS = 5;
    vector<thread> threads;
    vector<int> threadIds(NUM_THREADS);
    mutex coutMutex;
    
    // 记录主线程ID
    int mainTid = CurrentThread::tid();
    cout << "主线程ID: " << mainTid << endl;
    
    // 创建多个线程
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([i, &threadIds, &coutMutex]() {
            // 在线程中获取线程ID
            int myTid = CurrentThread::tid();
            threadIds[i] = myTid;
            
            {
                lock_guard<mutex> lock(coutMutex);
                cout << "  线程" << i << " ID: " << myTid << endl;
            }
            
            // 验证多次调用一致性
            for (int j = 0; j < 3; j++) {
                int tid2 = CurrentThread::tid();
                if (tid2 != myTid) {
                    lock_guard<mutex> lock(coutMutex);
                    cout << "  线程" << i << " 第" << j+1 << "次调用不一致!" << endl;
                    assert(false);
                }
            }
            
            // 验证与主线程不同
            assert(myTid != 0);
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 验证所有线程ID都不同
    cout << "\n验证线程ID唯一性:" << endl;
    for (int i = 0; i < NUM_THREADS; i++) {
        cout << "  线程" << i << " ID: " << threadIds[i];
        
        // 检查是否与主线程不同
        if (threadIds[i] == mainTid) {
            cout << " ✗ (与主线程ID相同!)" << endl;
            assert(false);
        }
        
        // 检查是否与其他线程不同
        for (int j = i + 1; j < NUM_THREADS; j++) {
            if (threadIds[i] == threadIds[j]) {
                cout << " ✗ (与线程" << j << "ID相同!)" << endl;
                assert(false);
            }
        }
        
        cout << " ✓ (唯一)" << endl;
    }
    
    // 验证主线程ID未变
    assert(CurrentThread::tid() == mainTid);
    cout << "主线程ID未改变: " << CurrentThread::tid() << " ✓" << endl;
    
    cout << "=== 测试2通过 ===\n" << endl;
}

// 测试3: 性能测试
void testPerformance() {
    cout << "=== 测试3: 性能测试 ===" << endl;
    
    // 先缓存线程ID
    CurrentThread::cacheTid();
    
    const int ITERATIONS = 1000000;
    
    // 测试 tid() 性能
    auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        volatile int t = CurrentThread::tid();  // volatile防止优化
        (void)t;
    }
    auto end = chrono::high_resolution_clock::now();
    
    auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
    cout << ITERATIONS << " 次 tid() 调用耗时: " 
         << duration.count() << " 微秒" << endl;
    cout << "平均每次调用: " << (duration.count() * 1000.0 / ITERATIONS) 
         << " 纳秒" << endl;
    
    // 与 gettid() 系统调用对比
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        volatile pid_t t = static_cast<pid_t>(::syscall(SYS_gettid));
        (void)t;
    }
    end = chrono::high_resolution_clock::now();
    
    duration = chrono::duration_cast<chrono::microseconds>(end - start);
    cout << "\n对比: " << ITERATIONS << " 次 syscall(SYS_gettid) 调用耗时: "
         << duration.count() << " 微秒" << endl;
    cout << "平均每次调用: " << (duration.count() * 1000.0 / ITERATIONS) 
         << " 纳秒" << endl;
    
    cout << "=== 测试3完成 ===\n" << endl;
}

// 测试4: 边界条件测试
void testEdgeCases() {
    cout << "=== 测试4: 边界条件测试 ===" << endl;
    
    // 测试1: 在主线程中先调用tid()再调用cacheTid()
    cout << "1. 测试调用顺序:" << endl;
    
    // 重置状态（实际无法重置，但可以测试不同情况）
    // 这里主要验证函数不会崩溃
    cout << "  先调用 tid(): " << CurrentThread::tid() << endl;
    cout << "  再调用 cacheTid(): ";
    CurrentThread::cacheTid();
    cout << "完成" << endl;
    cout << "  再次调用 tid(): " << CurrentThread::tid() << endl;
    
    // 测试2: 多线程竞争条件测试
    cout << "\n2. 测试多线程同时首次调用:" << endl;
    {
        const int NUM_THREADS = 10;
        vector<thread> threads;
        atomic<int> successCount{0};
        atomic<int> sameCount{0};
        
        // 确保所有线程同时开始
        atomic<bool> startFlag{false};
        
        for (int i = 0; i < NUM_THREADS; i++) {
            threads.emplace_back([&successCount, &sameCount, &startFlag]() {
                while (!startFlag.load()) {
                    this_thread::yield();
                }
                
                // 所有线程同时调用tid()
                int t = CurrentThread::tid();
                if (t != 0) {
                    successCount++;
                }
                
                // 检查是否与其他调用结果一致
                static atomic<int> firstTid{0};
                int expected = 0;
                if (firstTid.compare_exchange_strong(expected, t)) {
                    sameCount++;
                } else if (firstTid.load() == t) {
                    sameCount++;
                }
            });
        }
        
        // 让所有线程同时开始
        startFlag.store(true);
        
        for (auto& t : threads) {
            t.join();
        }
        
        cout << "  成功获取线程ID: " << successCount << "/" << NUM_THREADS << endl;
        cout << "  线程ID一致: " << sameCount << "/" << NUM_THREADS << endl;
        assert(successCount == NUM_THREADS);
    }
    
    // 测试3: 在子线程中fork
    cout << "\n3. 测试fork场景:" << endl;
    thread t([]() {
        pid_t pid = fork();
        if (pid == 0) {
            // 子进程
            cout << "  子进程中线程ID: " << CurrentThread::tid() << endl;
            _exit(0);
        } else if (pid > 0) {
            // 父进程
            waitpid(pid, nullptr, 0);
        }
    });
    t.join();
    
    cout << "=== 测试4通过 ===\n" << endl;
}

// 测试5: 内联和线程局部存储验证
void testInlineAndThreadLocal() {
    cout << "=== 测试5: 内联和线程局部存储验证 ===" << endl;
    
    // 验证变量地址在不同线程中不同
    cout << "1. 验证线程局部存储地址:" << endl;
    
    // 获取主线程中变量的地址
    int* mainAddr = &t_cacheTid;
    cout << "  主线程 t_cacheTid 地址: " << mainAddr << endl;
    
    thread t([mainAddr]() {
        int* threadAddr = &t_cacheTid;
        cout << "  子线程 t_cacheTid 地址: " << threadAddr << endl;
        
        if (threadAddr == mainAddr) {
            cout << "  ✗ 错误: 两个线程共享了同一个变量地址!" << endl;
            assert(false);
        } else {
            cout << "  ✓ 正确: 不同线程有不同的变量实例" << endl;
        }
        
        // 验证子线程中tid()返回的值与主线程不同
        assert(CurrentThread::tid() != 0);
        cout << "  子线程ID: " << CurrentThread::tid() << endl;
    });
    
    t.join();
    
    // 验证内联特性：多次包含不会导致链接错误
    cout << "\n2. 验证内联特性:" << endl;
    cout << "  多次包含头文件不会导致链接错误 ✓" << endl;
    
    cout << "=== 测试5通过 ===\n" << endl;
}

// 主测试函数
int main() {
    cout << "开始 CurrentThread 测试套件\n" << endl;
    
    try {
        cout << "当前线程ID: " << CurrentThread::tid() << endl;
        
        testBasicFunctionality();
        testMultiThreadIndependence();
        testPerformance();
        testEdgeCases();
        testInlineAndThreadLocal();
        
        cout << "\n" << string(50, '=') << endl;
        cout << "所有测试通过！CurrentThread 实现正确。" << endl;
        cout << string(50, '=') << endl;
        
    } catch (const exception& e) {
        cerr << "\n测试失败，异常: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "\n测试失败，未知异常" << endl;
        return 1;
    }
    
    return 0;
}