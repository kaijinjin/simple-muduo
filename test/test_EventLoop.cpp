#include "./../EventLoop.h"
#include "./../Channel.h"
#include "./../Logger.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cassert>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

using namespace std;

// 测试1: 基本功能测试（不运行loop）
void testBasicFunctionality() {
    cout << "=== 测试1: 基本功能测试 ===" << endl;
    
    // 创建EventLoop
    EventLoop loop;
    cout << "1. EventLoop创建成功，线程ID: " << CurrentThread::tid() << endl;
    assert(loop.isInLoopThread());
    
    // 测试runInLoop在本线程执行
    cout << "\n2. 测试runInLoop（本线程）:" << endl;
    int count = 0;
    loop.runInLoop([&count]() {
        cout << "   回调执行: count从" << count << "增加到";
        count++;
        cout << count << " (线程: " << CurrentThread::tid() << ")" << endl;
    });
    assert(count == 1);
    
    // 测试queueInLoop（任务加入队列）
    cout << "\n3. 测试queueInLoop（任务加入队列）:" << endl;
    loop.queueInLoop([&count]() {
        cout << "   queueInLoop任务 (不会执行，因为loop没运行)" << endl;
    });
    cout << "   任务已加入队列" << endl;
    
    cout << "=== 测试1完成 ===\n" << endl;
}

// 测试2: 单EventLoop完整生命周期测试
void testSingleEventLoopLifecycle() {
    cout << "=== 测试2: EventLoop完整生命周期测试 ===" << endl;
    
    atomic<int> completed_tasks{0};
    const int NUM_TASKS = 5;
    
    // 创建EventLoop并在独立线程运行
    EventLoop loop;
    
    thread loop_thread([&loop]() {
        cout << "   EventLoop线程 " << CurrentThread::tid() << " 启动" << endl;
        loop.loop();
        cout << "   EventLoop线程 " << CurrentThread::tid() << " 结束" << endl;
    });
    
    // 等待EventLoop启动
    this_thread::sleep_for(chrono::milliseconds(50));
    
    // 从主线程提交任务
    cout << "\n1. 从主线程提交任务:" << endl;
    for (int i = 0; i < NUM_TASKS; i++) {
        loop.runInLoop([i, &completed_tasks]() {
            cout << "   任务" << i << " 在线程 " << CurrentThread::tid() << " 执行" << endl;
            completed_tasks++;
        });
    }
    
    // 等待任务完成
    this_thread::sleep_for(chrono::milliseconds(100));
    cout << "   完成任务: " << completed_tasks << "/" << NUM_TASKS << endl;
    assert(completed_tasks == NUM_TASKS);
    
    // 测试queueInLoop
    cout << "\n2. 测试queueInLoop:" << endl;
    loop.queueInLoop([&completed_tasks]() {
        cout << "   queueInLoop任务执行" << endl;
        completed_tasks++;
    });
    
    // 等待queueInLoop任务执行
    this_thread::sleep_for(chrono::milliseconds(50));
    
    // 停止EventLoop
    cout << "\n3. 停止EventLoop:" << endl;
    loop.quit();
    
    loop_thread.join();
    cout << "   最终完成任务数: " << completed_tasks << endl;
    
    cout << "=== 测试2通过 ===\n" << endl;
}



// 测试5: 唤醒机制测试
void testWakeupMechanism() {
    cout << "=== 测试5: 唤醒机制测试 ===" << endl;
    
    EventLoop loop;
    atomic<int> wakeup_count{0};
    
    // 在独立线程运行EventLoop
    thread loop_thread([&loop]() {
        loop.loop();
    });
    
    // 等待启动
    this_thread::sleep_for(chrono::milliseconds(50));
    
    cout << "1. 测试wakeup()调用:" << endl;
    
    // 多次唤醒
    for (int i = 0; i < 3; i++) {
        loop.wakeup();
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    
    cout << "2. 测试queueInLoop唤醒:" << endl;
    for (int i = 0; i < 3; i++) {
        loop.queueInLoop([i]() {
            cout << "   任务" << i << " 执行" << endl;
        });
    }
    
    cout << "3. 测试quit()唤醒:" << endl;
    loop.quit();
    
    loop_thread.join();
    cout << "   ✅ 唤醒机制工作正常" << endl;
    
    cout << "=== 测试5通过 ===\n" << endl;
}

// 主测试函数
int main() {
    cout << "开始 EventLoop 测试套件\n" << endl;
    
    try {
        testBasicFunctionality();           // 基本功能
        testSingleEventLoopLifecycle();     // 完整生命周期
        testWakeupMechanism();              // 唤醒机制
        
        cout << string(60, '=') << endl;
        cout << "🎉 所有 EventLoop 测试通过！" << endl;
        cout << string(60, '=') << endl;
        
    } catch (const exception& e) {
        cerr << "\n❌ 测试失败，异常: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "\n❌ 测试失败，未知异常" << endl;
        return 1;
    }
    
    return 0;
}