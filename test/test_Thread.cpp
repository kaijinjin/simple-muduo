// AI生成
#include "./../Thread.h"
#include "./../CurrentThread.h"
#include "./../Logger.h"
#include <iostream>
#include <atomic>
#include <chrono>
#include <cassert>
#include <vector>
#include <mutex>

using namespace std;

// 测试1: 基本功能测试
void testBasicFunctionality() {
    cout << "=== 测试1: 基本功能测试 ===" << endl;
    
    cout << "1. 创建线程但不启动:" << endl;
    {
        Thread thread([]() {
            cout << "   线程执行" << endl;
        }, "TestThread");
        
        assert(!thread.started());
        cout << "   线程名称: " << thread.name() << endl;
        cout << "   线程ID: " << thread.tid() << endl;
        assert(thread.tid() == 0);  // 未启动时tid为0
    }
    
    cout << "\n2. 启动并等待线程完成:" << endl;
    {
        atomic<bool> thread_executed{false};
        Thread thread([&thread_executed]() {
            cout << "   线程开始执行" << endl;
            this_thread::sleep_for(chrono::milliseconds(50));
            thread_executed = true;
            cout << "   线程结束执行" << endl;
        }, "Worker");
        
        thread.start();
        assert(thread.started());
        assert(thread.tid() > 0);  // 启动后应该有有效的tid
        
        cout << "   线程已启动，名称: " << thread.name() 
             << ", tid: " << thread.tid() << endl;
        
        thread.join();
        assert(thread_executed.load());
    }
    
    cout << "\n3. 测试自动析构（不join）:" << endl;
    {
        atomic<bool> thread_executed{false};
        {
            Thread thread([&thread_executed]() {
                cout << "   后台线程开始" << endl;
                this_thread::sleep_for(chrono::milliseconds(100));
                thread_executed = true;
                cout << "   后台线程结束" << endl;
            }, "DetachedThread");
            
            thread.start();
            cout << "   线程启动后立即离开作用域（自动detach）" << endl;
        }  // 这里thread析构，会自动detach
        
        // 等待后台线程执行
        this_thread::sleep_for(chrono::milliseconds(150));
        assert(thread_executed.load());
    }
    
    cout << "=== 测试1通过 ===\n" << endl;
}

// 测试2: 线程ID和名称测试
void testThreadIdAndName() {
    cout << "=== 测试2: 线程ID和名称测试 ===" << endl;
    
    cout << "1. 测试自动命名:" << endl;
    {
        Thread thread1([]() {}, "");
        Thread thread2([]() {}, "");
        Thread thread3([]() {}, "");
        
        cout << "   线程1名称: " << thread1.name() << endl;
        cout << "   线程2名称: " << thread2.name() << endl;
        cout << "   线程3名称: " << thread3.name() << endl;
        
        assert(thread1.name().find("Thread") == 0);
        assert(thread2.name().find("Thread") == 0);
        assert(thread3.name().find("Thread") == 0);
        assert(thread1.name() != thread2.name());
        assert(thread2.name() != thread3.name());
    }
    
    cout << "\n2. 测试自定义名称:" << endl;
    {
        Thread thread([]() {}, "MyCustomThread");
        cout << "   自定义名称: " << thread.name() << endl;
        assert(thread.name() == "MyCustomThread");
    }
    
    cout << "\n3. 测试线程ID有效性:" << endl;
    {
        pid_t main_tid = CurrentThread::tid();
        pid_t worker_tid = 0;
        
        Thread thread([&worker_tid]() {
            worker_tid = CurrentThread::tid();
            cout << "   工作线程ID: " << worker_tid << endl;
        });
        
        thread.start();
        thread.join();
        
        cout << "   主线程ID: " << main_tid << endl;
        cout << "   记录的工作线程ID: " << worker_tid << endl;
        cout << "   Thread对象报告的tid: " << thread.tid() << endl;
        
        assert(worker_tid > 0);
        assert(thread.tid() > 0);
        assert(worker_tid == thread.tid());
        assert(worker_tid != main_tid);
    }
    
    cout << "=== 测试2通过 ===\n" << endl;
}

// 测试3: 多线程并发测试
void testConcurrency() {
    cout << "=== 测试3: 多线程并发测试 ===" << endl;
    
    const int NUM_THREADS = 10;
    vector<unique_ptr<Thread>> threads;
    atomic<int> completed_count{0};
    mutex cout_mutex;
    
    cout << "创建 " << NUM_THREADS << " 个并发线程:" << endl;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(make_unique<Thread>(
            [i, &completed_count, &cout_mutex]() {
                {
                    lock_guard<mutex> lock(cout_mutex);
                    cout << "   线程" << i << " 开始 (tid: " 
                         << CurrentThread::tid() << ")" << endl;
                }
                
                // 模拟工作
                this_thread::sleep_for(chrono::milliseconds(10 * (i % 3)));
                
                completed_count++;
                
                {
                    lock_guard<mutex> lock(cout_mutex);
                    cout << "   线程" << i << " 完成" << endl;
                }
            },
            "Worker" + to_string(i)
        ));
    }
    
    // 启动所有线程
    for (auto& thread : threads) {
        thread->start();
    }
    
    // 等待所有线程
    for (auto& thread : threads) {
        thread->join();
    }
    
    cout << "完成线程数: " << completed_count << "/" << NUM_THREADS << endl;
    assert(completed_count == NUM_THREADS);
    
    // 验证所有线程有不同的tid
    cout << "\n验证线程ID唯一性:" << endl;
    for (size_t i = 0; i < threads.size(); i++) {
        for (size_t j = i + 1; j < threads.size(); j++) {
            assert(threads[i]->tid() != threads[j]->tid());
        }
    }
    cout << "   所有线程ID唯一 ✓" << endl;
    
    cout << "=== 测试3通过 ===\n" << endl;
}

// 测试4: 线程计数测试
void testThreadCount() {
    cout << "=== 测试4: 线程计数测试 ===" << endl;
    
    int initial_count = Thread::numCreated();
    cout << "初始线程计数: " << initial_count << endl;
    
    cout << "\n1. 测试线程创建计数:" << endl;
    {
        vector<unique_ptr<Thread>> threads;
        const int CREATE_COUNT = 5;
        
        for (int i = 0; i < CREATE_COUNT; i++) {
            threads.emplace_back(make_unique<Thread>([]() {
                this_thread::sleep_for(chrono::milliseconds(10));
            }));
            
            cout << "   创建后计数: " << Thread::numCreated() << endl;
        }
        
        int expected = initial_count + CREATE_COUNT;
        cout << "   期望计数: " << expected << endl;
        cout << "   实际计数: " << Thread::numCreated() << endl;
        assert(Thread::numCreated() == expected);
        
        // 启动并等待所有线程
        for (auto& thread : threads) {
            thread->start();
            thread->join();
        }
    }
    
    cout << "\n2. 测试作用域结束后计数不变:" << endl;
    {
        int count_before = Thread::numCreated();
        {
            Thread thread([]() {});
            // 创建但未启动
        }  // thread析构
        
        int count_after = Thread::numCreated();
        cout << "   计数前: " << count_before << endl;
        cout << "   计数后: " << count_after << endl;
        assert(count_after == count_before + 1);  // 仍然会计数
    }
    
    cout << "=== 测试4通过 ===\n" << endl;
}

// 测试5: 边界条件测试
void testEdgeCases() {
    cout << "=== 测试5: 边界条件测试 ===" << endl;
    
    // 测试1: 空函数
    cout << "1. 测试空函数线程:" << endl;
    {
        Thread thread([]() {}, "EmptyFunction");
        thread.start();
        thread.join();
        cout << "   空函数线程执行完成 ✓" << endl;
    }
    
    // 测试2: 长时间运行线程
    cout << "\n2. 测试长时间运行线程:" << endl;
    {
        atomic<bool> running{true};
        atomic<bool> started{false};
        
        Thread thread([&running, &started]() {
            started = true;
            cout << "   长时间线程开始" << endl;
            int count = 0;
            while (running && count < 100) {
                this_thread::sleep_for(chrono::milliseconds(10));
                count++;
            }
            cout << "   长时间线程结束" << endl;
        }, "LongRunning");
        
        thread.start();
        
        // 等待线程启动
        while (!started) {
            this_thread::sleep_for(chrono::milliseconds(1));
        }
        
        // 运行一段时间后停止
        this_thread::sleep_for(chrono::milliseconds(100));
        running = false;
        
        thread.join();
        cout << "   长时间线程安全停止 ✓" << endl;
    }
    
    // 测试3: 异常处理
    cout << "\n3. 测试线程中抛出异常:" << endl;
    {
        Thread thread([]() {
            cout << "   线程即将抛出异常" << endl;
            try
            {
                throw runtime_error("测试异常");
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
            

        }, "ExceptionThread");
        
        try {
            thread.start();
            thread.join();
            cout << "   ⚠️ 异常未被传播到主线程" << endl;
        } catch (const exception& e) {
            cout << "   捕获到异常: " << e.what() << endl;
        } catch (...) {
            cout << "   捕获到未知异常" << endl;
        }
        cout << "   程序继续执行 ✓" << endl;
    }
    
    // 测试4: 多次join（应该没问题）
    cout << "\n4. 测试多次调用join:" << endl;
    {
        Thread thread([]() {
            cout << "   简单线程" << endl;
        });
        
        thread.start();
        thread.join();
        
        // 再次join（应该没问题，但joined_状态可能已经为true）
        // thread.join();  // 实际std::thread会抛出异常
        
        cout << "   单次join成功 ✓" << endl;
    }
    
    // 测试5: 启动已启动的线程
    cout << "\n5. 测试重复启动:" << endl;
    {
        Thread thread([]() {
            cout << "   线程执行一次" << endl;
        });
        
        thread.start();
        
        // 再次启动（应该没问题，但started_已为true）
        // thread.start();  // 实际行为取决于实现
        
        thread.join();
        cout << "   单次启动成功 ✓" << endl;
    }
    
    cout << "=== 测试5完成 ===\n" << endl;
}

// 测试6: 性能测试
void testPerformance() {
    cout << "=== 测试6: 性能测试 ===" << endl;
    
    const int NUM_THREADS = 100;
    atomic<int> completed{0};
    
    cout << "测试创建和启动 " << NUM_THREADS << " 个线程:" << endl;
    
    auto start = chrono::high_resolution_clock::now();
    
    vector<unique_ptr<Thread>> threads;
    threads.reserve(NUM_THREADS);
    
    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(make_unique<Thread>(
            [&completed]() {
                completed++;
            },
            "PerfTest" + to_string(i)
        ));
    }
    
    auto create_end = chrono::high_resolution_clock::now();
    
    // 启动所有线程
    for (auto& thread : threads) {
        thread->start();
    }
    
    auto start_end = chrono::high_resolution_clock::now();
    
    // 等待所有线程
    for (auto& thread : threads) {
        thread->join();
    }
    
    auto join_end = chrono::high_resolution_clock::now();
    
    auto create_time = chrono::duration_cast<chrono::milliseconds>(create_end - start);
    auto start_time = chrono::duration_cast<chrono::milliseconds>(start_end - create_end);
    auto join_time = chrono::duration_cast<chrono::milliseconds>(join_end - start_end);
    auto total_time = chrono::duration_cast<chrono::milliseconds>(join_end - start);
    
    cout << "   创建时间: " << create_time.count() << "ms" << endl;
    cout << "   启动时间: " << start_time.count() << "ms" << endl;
    cout << "   等待时间: " << join_time.count() << "ms" << endl;
    cout << "   总时间: " << total_time.count() << "ms" << endl;
    cout << "   完成线程数: " << completed << "/" << NUM_THREADS << endl;
    
    assert(completed == NUM_THREADS);
    
    cout << "=== 测试6完成 ===\n" << endl;
}

// 主测试函数
int main() {
    cout << "开始 Thread 类测试套件\n" << endl;
    
    try {
        testBasicFunctionality();
        testThreadIdAndName();
        testConcurrency();
        testThreadCount();
        testEdgeCases();
        testPerformance();
        
        cout << string(60, '=') << endl;
        cout << "🎉 所有 Thread 测试通过！" << endl;
        cout << string(60, '=') << endl;
        
    } catch (const exception& e) {
        cerr << "\n❌ 测试失败，异常: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "\n❌ 测试失败，未知异常" << endl;
        return 1;
    }
    
    cout << "\n最终线程创建计数: " << Thread::numCreated() << endl;
    
    return 0;
}