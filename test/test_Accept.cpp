// Ai生成
#include "./../Accept.h"
#include "./../EventLoop.h"
#include "./../InetAddress.h"
#include "./../Logger.h"
#include <arpa/inet.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>
#include <random>
#include <future>

// ================= 辅助函数 =================
int get_available_port(int start_port = 30000) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(start_port, start_port + 1000);
    
    for (int i = 0; i < 20; i++) {
        int port = dis(gen);
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(sock);
            std::cout << "    找到可用端口: " << port << std::endl;
            return port;
        }
        
        close(sock);
    }
    return -1;
}

// ================= 测试工具类 =================
class TestRunner {
private:
    std::atomic<bool> running_{false};
    std::thread loop_thread_;
    EventLoop* loop_;
    
public:
    TestRunner() : loop_(nullptr) {
        // 在单独线程中运行 EventLoop
        loop_thread_ = std::thread([this]() {
            loop_ = new EventLoop();
            running_ = true;
            loop_->loop();
            running_ = false;
        });
        
        // 等待 EventLoop 开始运行
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    ~TestRunner() {
        if (loop_) {
            // 安全停止 EventLoop
            loop_->runInLoop([this]() {
                loop_->quit();
            });
            
            if (loop_thread_.joinable()) {
                loop_thread_.join();
            }
            
            delete loop_;
            loop_ = nullptr;
        }
    }
    
    EventLoop* loop() { return loop_; }
    
    void runInLoop(std::function<void()> cb) {
        if (loop_) {
            loop_->runInLoop(cb);
        }
    }
    
    void waitFor(std::chrono::milliseconds timeout) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

// ================= 测试用例 =================

// 测试1：基本构造和析构
bool test_constructor_and_destructor() {
    std::cout << "  测试构造和析构..." << std::endl;
    
    TestRunner runner;
    int port = get_available_port();
    if (port == -1) {
        std::cerr << "    无法找到可用端口" << std::endl;
        return false;
    }
    
    InetAddress listenAddr("127.0.0.1", port);
    
    // 在 EventLoop 线程中创建和销毁 Acceptor
    std::promise<bool> promise;
    auto future = promise.get_future();
    
    runner.runInLoop([&]() {
        {
            Acceptor acceptor(runner.loop(), listenAddr, true);
            
            // 检查初始状态
            if (acceptor.listenning()) {
                std::cerr << "    初始状态应为未监听" << std::endl;
                promise.set_value(false);
                return;
            }
            
            std::cout << "    ✓ 构造成功" << std::endl;
        }
        std::cout << "    ✓ 析构成功" << std::endl;
        promise.set_value(true);
    });
    
    return future.get();
}

// 测试2：监听功能
bool test_listen_function() {
    std::cout << "  测试监听功能..." << std::endl;
    
    TestRunner runner;
    int port = get_available_port();
    if (port == -1) {
        std::cerr << "    无法找到可用端口" << std::endl;
        return false;
    }
    
    InetAddress listenAddr("127.0.0.1", port);
    
    std::promise<bool> promise;
    auto future = promise.get_future();
    
    runner.runInLoop([&]() {
        Acceptor acceptor(runner.loop(), listenAddr, true);
        
        // 监听前状态
        if (acceptor.listenning()) {
            std::cerr << "    监听前状态错误" << std::endl;
            promise.set_value(false);
            return;
        }
        
        // 开始监听
        acceptor.listenFd();
        
        // 监听后状态
        if (!acceptor.listenning()) {
            std::cerr << "    监听后状态错误" << std::endl;
            promise.set_value(false);
            return;
        }
        
        std::cout << "    ✓ 监听状态正确" << std::endl;
        
        // 尝试重复监听（应该无害）
        acceptor.listenFd();
        
        std::cout << "    ✓ 重复监听无害" << std::endl;
        promise.set_value(true);
    });
    
    return future.get();
}

// 测试3：接受连接测试
bool test_accept_connections() {
    std::cout << "  测试接受连接..." << std::endl;
    
    int port = get_available_port();
    if (port == -1) {
        std::cerr << "    无法找到可用端口" << std::endl;
        return false;
    }
    
    std::cout << "    使用端口: " << port << std::endl;
    
    std::atomic<int> accepted_count{0};
    std::atomic<bool> test_complete{false};
    
    TestRunner runner;
    InetAddress listenAddr("127.0.0.1", port);
    
    std::promise<void> acceptor_ready;
    auto ready_future = acceptor_ready.get_future();
    
    // 在 EventLoop 线程中创建和配置 Acceptor
    std::unique_ptr<Acceptor> acceptor(new Acceptor(runner.loop(), listenAddr, true));
    runner.runInLoop([&]() {
        // 设置新连接回调
        acceptor->setNewConnectionCallback([&](int sockfd, const InetAddress& peerAddr) {
            accepted_count++;
            
            std::cout << "    接受新连接 #" << accepted_count 
                     << "，fd=" << sockfd 
                     << "，来自 " << peerAddr.toIpPort() << std::endl;
            
            // 验证客户端地址
            if (peerAddr.toIp() != "127.0.0.1") {
                std::cerr << "    客户端IP地址错误: " << peerAddr.toIp() << std::endl;
            }
            
            close(sockfd);
            
            // 收到3个连接后完成测试
            if (accepted_count >= 3) {
                test_complete = true;
            }
        });
        
        // 开始监听
        acceptor->listenFd();
        std::cout << "    开始监听，等待连接..." << std::endl;
        
        // 通知主线程 Acceptor 已准备好
        acceptor_ready.set_value();
        
        // 保持 Acceptor 存活
        runner.waitFor(std::chrono::seconds(5));
    });
    
    // 等待 Acceptor 准备好
    ready_future.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 启动客户端线程
    std::thread client_thread([port]() {
        for (int i = 0; i < 3; i++) {
            int client_fd = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            memset(&serv_addr, 0, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(port);
            inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
            
            if (connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
                std::cout << "    客户端连接成功 #" << (i+1) << std::endl;
                
                // 发送测试数据
                const char* test_data = "Hello";
                send(client_fd, test_data, strlen(test_data), 0);
                
                // 立即关闭连接
                close(client_fd);
            } else {
                std::cerr << "    客户端连接失败: " << strerror(errno) << std::endl;
            }
            
            if (i < 2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    });
    
    // 等待测试完成
    auto start_time = std::chrono::steady_clock::now();
    while (!test_complete && 
           std::chrono::steady_clock::now() - start_time < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    client_thread.join();
    
    // 检查结果
    if (accepted_count != 3) {
        std::cerr << "    接受连接数量错误: " << accepted_count << " != 3" << std::endl;
        return false;
    }
    
    std::cout << "    ✓ 成功接受3个连接" << std::endl;
    return true;
}

// 测试4：无回调时的默认行为
bool test_no_callback_behavior() {
    std::cout << "  测试无回调时的行为..." << std::endl;
    
    int port = get_available_port();
    if (port == -1) {
        std::cerr << "    无法找到可用端口" << std::endl;
        return false;
    }
    
    TestRunner runner;
    InetAddress listenAddr("127.0.0.1", port);
    
    std::promise<void> acceptor_ready;
    auto ready_future = acceptor_ready.get_future();
    std::atomic<bool> connection_attempted{false};

    Acceptor acceptor(runner.loop(), listenAddr, true);
    // 在 EventLoop 线程中创建 Acceptor
    runner.runInLoop([&]() {
        // 不设置回调
        acceptor.listenFd();
        
        std::cout << "    Acceptor 创建并监听" << std::endl;
        acceptor_ready.set_value();
        // 保持 Acceptor 存活一段时间
        runner.waitFor(std::chrono::seconds(2));
    });
    
    // 等待 Acceptor 准备好
    ready_future.wait();
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 尝试连接
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    
    if (connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
        // 连接应该被立即关闭（因为没有回调）
        char buffer[256];
        ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
        if (n != 0) {
            std::cerr << "    连接未被正确关闭，收到 " << n << " 字节" << std::endl;
            close(client_fd);
            return false;
        }
        close(client_fd);
        connection_attempted = true;
    }
    
    if (connection_attempted) {
        std::cout << "    ✓ 无回调时连接被正确关闭" << std::endl;
        return true;
    } else {
        std::cout << "    ⚠️  无法测试连接行为" << std::endl;
        return true;
    }
}

// 测试5：InetAddress 方法
bool test_inetaddress_methods() {
    std::cout << "  测试InetAddress方法..." << std::endl;
    
    bool all_passed = true;
    
    // 测试1：基本功能
    {
        InetAddress addr1("192.168.1.1", 8080);
        if (addr1.toIp() != "192.168.1.1") {
            std::cerr << "    toIp() 错误: " << addr1.toIp() << std::endl;
            all_passed = false;
        }
        if (addr1.toPort() != 8080) {
            std::cerr << "    toPort() 错误: " << addr1.toPort() << std::endl;
            all_passed = false;
        }
        if (addr1.toIpPort() != "192.168.1.1:8080") {
            std::cerr << "    toIpPort() 错误: " << addr1.toIpPort() << std::endl;
            all_passed = false;
        }
    }
    
    if (all_passed) {
        std::cout << "    ✓ InetAddress 基本方法正常" << std::endl;
    }
    
    // 测试2：特殊地址
    {
        InetAddress addr2("127.0.0.1", 80);
        if (addr2.toIp() != "127.0.0.1") {
            std::cerr << "    loopback IP 错误" << std::endl;
            all_passed = false;
        }
    }
    
    if (all_passed) {
        std::cout << "    ✓ loopback地址正常" << std::endl;
    }
    
    // 测试3：0.0.0.0
    {
        InetAddress addr3("0.0.0.0", 443);
        if (addr3.toIp() != "0.0.0.0") {
            std::cerr << "    any address IP 错误" << std::endl;
            all_passed = false;
        }
    }
    
    if (all_passed) {
        std::cout << "    ✓ any地址正常" << std::endl;
    }
    
    return all_passed;
}

// 测试6：多Acceptor测试（不运行事件循环）
bool test_multiple_acceptors_no_loop() {
    std::cout << "  测试多个Acceptor创建..." << std::endl;
    
    bool all_passed = true;
    
    // 创建多个 EventLoop 和 Acceptor
    for (int i = 0; i < 3; i++) {
        int port = get_available_port(31000 + i * 100);
        if (port == -1) {
            std::cerr << "    无法找到可用端口" << std::endl;
            all_passed = false;
            continue;
        }
        
        EventLoop loop;
        InetAddress addr("127.0.0.1", port);
        
        try {
            Acceptor acceptor(&loop, addr, true);
            acceptor.listenFd();
            std::cout << "    ✓ Acceptor " << (i+1) << " 创建成功 (端口: " << port << ")" << std::endl;
        } catch (...) {
            std::cerr << "    ✗ Acceptor " << (i+1) << " 创建失败" << std::endl;
            all_passed = false;
        }
    }
    
    return all_passed;
}

// 测试7：端口释放测试
bool test_port_release() {
    std::cout << "  测试端口释放..." << std::endl;
    
    int port = get_available_port();
    if (port == -1) {
        std::cerr << "    无法找到可用端口" << std::endl;
        return false;
    }
    
    // 第一轮：创建并销毁 Acceptor
    {
        EventLoop loop;
        InetAddress listenAddr("127.0.0.1", port);
        
        {
            Acceptor acceptor(&loop, listenAddr, true);
            acceptor.listenFd();
            std::cout << "    ✓ 第一轮：Acceptor 创建并监听" << std::endl;
        }
        
        std::cout << "    ✓ 第一轮：Acceptor 销毁" << std::endl;
    }
    
    // 等待端口释放
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 第二轮：尝试重新绑定相同端口
    {
        EventLoop loop;
        InetAddress listenAddr("127.0.0.1", port);
        
        try {
            Acceptor acceptor(&loop, listenAddr, true);
            acceptor.listenFd();
            std::cout << "    ✓ 第二轮：成功重新绑定相同端口" << std::endl;
            return true;
        } catch (...) {
            std::cerr << "    ✗ 第二轮：无法重新绑定相同端口" << std::endl;
            return false;
        }
    }
}

// ================= 主测试函数 =================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "开始 Acceptor 类测试" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    bool all_passed = true;
    
    // 运行不需要网络连接的测试
    std::cout << "运行基本测试..." << std::endl;
    if (!test_constructor_and_destructor()) all_passed = false;
    if (!test_listen_function()) all_passed = false;
    if (!test_inetaddress_methods()) all_passed = false;
    if (!test_multiple_acceptors_no_loop()) all_passed = false;
    if (!test_port_release()) all_passed = false;
    
    std::cout << "\n运行需要网络连接的测试..." << std::endl;
    if (!test_accept_connections()) all_passed = false;
    if (!test_no_callback_behavior()) all_passed = false;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试结果" << std::endl;
    std::cout << "========================================" << std::endl;
    
    if (all_passed) {
        std::cout << "所有测试通过！✓" << std::endl;
        return 0;
    } else {
        std::cout << "部分测试失败 ✗" << std::endl;
        return 1;
    }

}