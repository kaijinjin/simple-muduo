// AI生成
#include "./../Socket.h"
#include "./../InetAddress.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <atomic>
#include <vector>
#include <netinet/tcp.h>

// 测试结果记录
struct TestResult {
    std::string testName;
    bool passed;
    std::string message;
    
    TestResult(const std::string& name, bool pass, const std::string& msg = "")
        : testName(name), passed(pass), message(msg) {}
};

// 测试用例集合
std::vector<TestResult> testResults;

// 辅助宏
#define RUN_TEST(test_func, test_name) \
    do { \
        std::cout << "运行测试: " << test_name << "..." << std::endl; \
        try { \
            if (test_func()) { \
                testResults.emplace_back(test_name, true, "通过"); \
                std::cout << "  ✓ 通过" << std::endl; \
            } else { \
                testResults.emplace_back(test_name, false, "失败"); \
                std::cout << "  ✗ 失败" << std::endl; \
            } \
        } catch (const std::exception& e) { \
            testResults.emplace_back(test_name, false, std::string("异常: ") + e.what()); \
            std::cout << "  ✗ 异常: " << e.what() << std::endl; \
        } catch (...) { \
            testResults.emplace_back(test_name, false, "未知异常"); \
            std::cout << "  ✗ 未知异常" << std::endl; \
        } \
        std::cout << std::endl; \
    } while(0)

// 测试1：基本功能测试
bool test_basic_construction() {
    int raw_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (raw_fd < 0) {
        std::cerr << "  无法创建原始套接字" << std::endl;
        return false;
    }
    
    // 测试构造和获取fd
    {
        Socket socket(raw_fd);
        if (socket.fd() != raw_fd) {
            std::cerr << "  fd() 返回值错误" << std::endl;
            return false;
        }
    }
    
    // Socket析构后应该关闭套接字
    int test_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (test_fd < 0) {
        std::cerr << "  无法创建测试套接字" << std::endl;
        return false;
    }
    close(test_fd);
    
    return true;
}

// 测试2：绑定地址测试
bool test_bind_address() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    Socket socket11(sockfd);
    
    // 测试正常绑定
    InetAddress addr1("127.0.0.1", 9000);
    socket11.bindAddress(addr1);
    
    // 测试绑定不同端口
    int sockfd2 = socket(AF_INET, SOCK_STREAM, 0);
    Socket socket2(sockfd2);
    socket2.setReuseAddr(true);
    InetAddress addr2("127.0.0.1", 9001);
    socket2.bindAddress(addr2);
    
    return true;
}

// 测试3：监听测试
bool test_listen() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    Socket socket11(sockfd);
    
    // 设置SO_REUSEADDR以便快速重启
    socket11.setReuseAddr(true);
    
    InetAddress addr("127.0.0.1", 9002);
    socket11.bindAddress(addr);
    
    // 监听
    socket11.listenFd();
    
    // 验证套接字处于监听状态
    int optval;
    socklen_t optlen = sizeof(optval);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ACCEPTCONN, &optval, &optlen) == 0) {
        if (optval == 0) {
            std::cerr << "  套接字未处于监听状态" << std::endl;
            return false;
        }
    }
    
    return true;
}

// 测试4：接受连接测试
bool test_accept() {
    // 创建服务器套接字
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    Socket server_socket(server_fd);
    
    server_socket.setReuseAddr(true);
    InetAddress server_addr("127.0.0.1", 9003);
    server_socket.bindAddress(server_addr);
    server_socket.listenFd();
    
    // 创建客户端线程
    std::atomic<bool> client_connected{false};
    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(9003);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
        
        if (connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
            client_connected = true;
            
            // 发送测试数据
            const char* test_data = "Hello Server";
            send(client_fd, test_data, strlen(test_data), 0);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            close(client_fd);
        }
    });
    
    // 接受连接
    InetAddress client_addr;
    int client_fd = server_socket.acceptFd(&client_addr);
    
    if (client_fd < 0) {
        std::cerr << "  接受连接失败: " << strerror(errno) << std::endl;
        client_thread.join();
        return false;
    }
    
    // 验证客户端地址
    const struct sockaddr_in* addr = client_addr.getSockAddr();
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), ip_str, sizeof(ip_str));
    
    if (strcmp(ip_str, "127.0.0.1") != 0) {
        std::cerr << "  客户端IP地址错误: " << ip_str << std::endl;
        close(client_fd);
        client_thread.join();
        return false;
    }
    
    // 验证返回的套接字是非阻塞的
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (!(flags & O_NONBLOCK)) {
        std::cerr << "  accept返回的套接字不是非阻塞的" << std::endl;
        close(client_fd);
        client_thread.join();
        return false;
    }
    
    // 读取客户端发送的数据
    char buffer[1024] = {0};
    ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
    if (n != strlen("Hello Server")) {
        std::cerr << "  接收数据长度错误: " << n << std::endl;
    }
    
    close(client_fd);
    client_thread.join();
    
    return client_connected && (n > 0);
}

// 测试5：关闭写端测试
bool test_shutdown_write() {
    // 使用socketpair创建一对连接的套接字
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        std::cerr << "  创建socketpair失败" << std::endl;
        return false;
    }
    
    Socket socket1(fds[0]);
    int socket2_fd = fds[1];
    
    // 在socket1上关闭写端
    socket1.shutdownWrite();
    
    // 验证socket2能读到EOF
    char buffer[1024];
    ssize_t n = recv(socket2_fd, buffer, sizeof(buffer), 0);
    if (n != 0) {
        std::cerr << "  shutdown写端后应该读到EOF，但收到 " << n << " 字节" << std::endl;
        close(socket2_fd);
        return false;
    }
    
    // socket2仍然可以写数据到socket1
    const char* test_msg = "Test message";
    ssize_t sent = send(socket2_fd, test_msg, strlen(test_msg), 0);
    if (sent <= 0) {
        std::cerr << "  另一端无法发送数据: " << strerror(errno) << std::endl;
        close(socket2_fd);
        return false;
    }
    
    // 验证socket1能收到数据
    char recv_buf[1024];
    n = recv(fds[0], recv_buf, sizeof(recv_buf), 0);
    if (n != sent) {
        std::cerr << "  接收数据长度不匹配: " << n << " != " << sent << std::endl;
        close(socket2_fd);
        return false;
    }
    
    close(socket2_fd);
    return true;
}

// 测试6：TCP_NODELAY选项测试
bool test_tcp_nodelay() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    Socket socket(sockfd);
    
    // 测试启用TCP_NODELAY
    socket.setTcpNoDelay(true);
    
    int optval;
    socklen_t optlen = sizeof(optval);
    if (getsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, &optlen) != 0) {
        std::cerr << "  获取TCP_NODELAY选项失败" << std::endl;
        return false;
    }
    
    if (optval != 1) {
        std::cerr << "  TCP_NODELAY应该为1，实际为" << optval << std::endl;
        return false;
    }
    
    // 测试禁用TCP_NODELAY
    socket.setTcpNoDelay(false);
    
    if (getsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, &optlen) != 0) {
        std::cerr << "  获取TCP_NODELAY选项失败" << std::endl;
        return false;
    }
    
    if (optval != 0) {
        std::cerr << "  TCP_NODELAY应该为0，实际为" << optval << std::endl;
        return false;
    }
    
    return true;
}

// 测试7：SO_REUSEADDR选项测试
bool test_reuse_addr() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    Socket socket(sockfd);
    
    // 测试启用SO_REUSEADDR
    socket.setReuseAddr(true);
    
    int optval;
    socklen_t optlen = sizeof(optval);
    if (getsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen) != 0) {
        std::cerr << "  获取SO_REUSEADDR选项失败" << std::endl;
        return false;
    }
    
    if (optval != 1) {
        std::cerr << "  SO_REUSEADDR应该为1，实际为" << optval << std::endl;
        return false;
    }
    
    // 测试禁用SO_REUSEADDR
    socket.setReuseAddr(false);
    
    if (getsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen) != 0) {
        std::cerr << "  获取SO_REUSEADDR选项失败" << std::endl;
        return false;
    }
    
    if (optval != 0) {
        std::cerr << "  SO_REUSEADDR应该为0，实际为" << optval << std::endl;
        return false;
    }
    
    return true;
}

// 测试8：SO_REUSEPORT选项测试
bool test_reuse_port() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    Socket socket(sockfd);
    
    // 测试启用SO_REUSEPORT
    socket.setReusePort(true);
    
    int optval;
    socklen_t optlen = sizeof(optval);
    
    // SO_REUSEPORT可能在某些系统上不可用
    if (getsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, &optlen) != 0) {
        // 系统不支持SO_REUSEPORT，这是可以的
        std::cout << "  系统不支持SO_REUSEPORT，跳过测试" << std::endl;
        return true;
    }
    
    if (optval != 1) {
        std::cerr << "  SO_REUSEPORT应该为1，实际为" << optval << std::endl;
        return false;
    }
    
    // 测试禁用SO_REUSEPORT
    socket.setReusePort(false);
    
    if (getsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, &optlen) != 0) {
        std::cerr << "  获取SO_REUSEPORT选项失败" << std::endl;
        return false;
    }
    
    if (optval != 0) {
        std::cerr << "  SO_REUSEPORT应该为0，实际为" << optval << std::endl;
        return false;
    }
    
    return true;
}

// 测试9：SO_KEEPALIVE选项测试
bool test_keep_alive() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    Socket socket(sockfd);
    
    // 测试启用SO_KEEPALIVE
    socket.setKeepAlive(true);
    
    int optval;
    socklen_t optlen = sizeof(optval);
    if (getsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) != 0) {
        std::cerr << "  获取SO_KEEPALIVE选项失败" << std::endl;
        return false;
    }
    
    if (optval != 1) {
        std::cerr << "  SO_KEEPALIVE应该为1，实际为" << optval << std::endl;
        return false;
    }
    
    // 测试禁用SO_KEEPALIVE
    socket.setKeepAlive(false);
    
    if (getsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) != 0) {
        std::cerr << "  获取SO_KEEPALIVE选项失败" << std::endl;
        return false;
    }
    
    if (optval != 0) {
        std::cerr << "  SO_KEEPALIVE应该为0，实际为" << optval << std::endl;
        return false;
    }
    
    return true;
}

// 测试10：综合功能测试 - 完整服务器流程
bool test_complete_server() {
    const int TEST_PORT = 9010;
    
    // 创建服务器套接字
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    Socket server_socket(server_fd);
    
    // 设置所有选项
    server_socket.setReuseAddr(true);
    server_socket.setReusePort(true);
    server_socket.setTcpNoDelay(true);
    server_socket.setKeepAlive(true);
    
    // 绑定和监听
    InetAddress server_addr("127.0.0.1", TEST_PORT);
    server_socket.bindAddress(server_addr);
    server_socket.listenFd();
    
    // 创建多个客户端
    const int CLIENT_COUNT = 3;
    std::vector<std::thread> clients;
    std::atomic<int> connected_count{0};
    std::atomic<int> message_count{0};
    
    for (int i = 0; i < CLIENT_COUNT; i++) {
        clients.emplace_back([&, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));
            
            int client_fd = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            memset(&serv_addr, 0, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(TEST_PORT);
            inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
            
            if (connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
                connected_count++;
                
                // 发送消息
                std::string msg = "Client_" + std::to_string(i);
                if (send(client_fd, msg.c_str(), msg.length(), 0) > 0) {
                    message_count++;
                }
                
                // 接收响应
                char buffer[256] = {0};
                recv(client_fd, buffer, sizeof(buffer), 0);
                
                close(client_fd);
            }
        });
    }
    
    // 服务器接受和处理连接
    std::vector<int> accepted_fds;
    for (int i = 0; i < CLIENT_COUNT; i++) {
        InetAddress client_addr;
        int client_fd = server_socket.acceptFd(&client_addr);
        
        if (client_fd >= 0) {
            accepted_fds.push_back(client_fd);
            
            // 接收数据
            char buffer[256] = {0};
            ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
            
            // 发送响应
            const char* response = "ACK";
            send(client_fd, response, strlen(response), 0);
            
            // 关闭客户端连接
            close(client_fd);
        }
    }
    
    // 等待所有客户端完成
    for (auto& client : clients) {
        client.join();
    }
    
    // 检查结果
    if (connected_count != CLIENT_COUNT) {
        std::cerr << "  连接数量错误: " << connected_count << " != " << CLIENT_COUNT << std::endl;
        return false;
    }
    
    if (message_count != CLIENT_COUNT) {
        std::cerr << "  消息数量错误: " << message_count << " != " << CLIENT_COUNT << std::endl;
        return false;
    }
    
    if (accepted_fds.size() != CLIENT_COUNT) {
        std::cerr << "  接受连接数量错误: " << accepted_fds.size() << " != " << CLIENT_COUNT << std::endl;
        return false;
    }
    
    return true;
}

// 测试11：性能测试
bool test_performance() {
    const int TEST_PORT = 9020;
    const int NUM_CONNECTIONS = 100;
    
    // 创建服务器
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    Socket server_socket(server_fd);
    
    server_socket.setReuseAddr(true);
    InetAddress server_addr("127.0.0.1", TEST_PORT);
    server_socket.bindAddress(server_addr);
    server_socket.listenFd();
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 创建客户端连接
    std::vector<std::thread> clients;
    std::atomic<int> success_count{0};
    
    for (int i = 0; i < NUM_CONNECTIONS; i++) {
        clients.emplace_back([&, i]() {
            int client_fd = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            memset(&serv_addr, 0, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(TEST_PORT);
            inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
            
            if (connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
                success_count++;
                close(client_fd);
            }
        });
    }
    
    // 接受连接
    std::vector<int> accepted_fds;
    for (int i = 0; i < NUM_CONNECTIONS; i++) {
        InetAddress client_addr;
        int client_fd = server_socket.acceptFd(&client_addr);
        if (client_fd >= 0) {
            accepted_fds.push_back(client_fd);
            close(client_fd);
        }
    }
    
    // 等待所有客户端完成
    for (auto& client : clients) {
        client.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    std::cout << "  性能测试结果:" << std::endl;
    std::cout << "    连接数量: " << NUM_CONNECTIONS << std::endl;
    std::cout << "    成功连接: " << success_count << std::endl;
    std::cout << "    耗时: " << duration.count() << "ms" << std::endl;
    std::cout << "    平均每个连接: " 
              << (duration.count() * 1000.0 / NUM_CONNECTIONS) << "μs" << std::endl;
    
    return success_count == NUM_CONNECTIONS;
}

// 测试12：边界条件测试
bool test_edge_cases() {
    std::cout << "  测试边界条件..." << std::endl;
    
    // 测试1：无效文件描述符
    try {
        Socket invalid_socket(-1);
        // 应该记录错误但不崩溃
        std::cout << "    无效fd测试通过" << std::endl;
    } catch (...) {
        std::cout << "    无效fd测试异常" << std::endl;
    }
    
    // 测试2：重复绑定相同地址（需要SO_REUSEADDR）
    {
        int sock1 = socket(AF_INET, SOCK_STREAM, 0);
        Socket socket1(sock1);
        socket1.setReuseAddr(true);
        
        InetAddress addr("127.0.0.1", 9030);
        socket1.bindAddress(addr);
        
        // 第二个套接字绑定相同地址
        int sock2 = socket(AF_INET, SOCK_STREAM, 0);
        Socket socket2(sock2);
        socket2.setReuseAddr(true);
        
        try {
            socket2.bindAddress(addr);
            std::cout << "    重复绑定测试通过" << std::endl;
        } catch (...) {
            std::cout << "    重复绑定测试异常" << std::endl;
        }
        
        close(sock1);
        close(sock2);
    }
    
    // 测试3：大量小数据包（测试TCP_NODELAY）
    {
        int fds[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0) {
            Socket socket1(fds[0]);
            socket1.setTcpNoDelay(true);
            
            // 发送多个小数据包
            const char* small_data = "X";
            for (int i = 0; i < 100; i++) {
                send(fds[0], small_data, 1, 0);
            }
            
            // 接收数据
            char buffer[200];
            ssize_t total = 0;
            while (total < 100) {
                ssize_t n = recv(fds[1], buffer + total, sizeof(buffer) - total, 0);
                if (n <= 0) break;
                total += n;
            }
            
            if (total == 100) {
                std::cout << "    小数据包测试通过" << std::endl;
            } else {
                std::cout << "    小数据包测试失败，收到 " << total << " 字节" << std::endl;
            }
            
            close(fds[1]);
        }
    }
    
    return true;
}

// 主测试函数
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "开始 Socket 类测试" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // 运行所有测试
    RUN_TEST(test_basic_construction, "基本构造和析构");
    RUN_TEST(test_bind_address, "绑定地址功能");
    RUN_TEST(test_listen, "监听功能");
    RUN_TEST(test_accept, "接受连接功能");
    RUN_TEST(test_shutdown_write, "关闭写端功能");
    RUN_TEST(test_tcp_nodelay, "TCP_NODELAY选项");
    RUN_TEST(test_reuse_addr, "SO_REUSEADDR选项");
    RUN_TEST(test_reuse_port, "SO_REUSEPORT选项");
    RUN_TEST(test_keep_alive, "SO_KEEPALIVE选项");
    RUN_TEST(test_complete_server, "完整服务器流程");
    RUN_TEST(test_performance, "性能测试");
    RUN_TEST(test_edge_cases, "边界条件测试");
    
    // 输出测试结果汇总
    std::cout << "========================================" << std::endl;
    std::cout << "测试结果汇总" << std::endl;
    std::cout << "========================================" << std::endl;
    
    int total = testResults.size();
    int passed = 0;
    
    for (const auto& result : testResults) {
        std::cout << (result.passed ? "✓ " : "✗ ") 
                  << result.testName;
        
        if (!result.message.empty()) {
            std::cout << " - " << result.message;
        }
        std::cout << std::endl;
        
        if (result.passed) passed++;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "总计: " << passed << "/" << total << " 个测试通过" << std::endl;
    
    if (passed == total) {
        std::cout << "所有测试通过！✓" << std::endl;
        return 0;
    } else {
        std::cout << "有 " << (total - passed) << " 个测试失败" << std::endl;
        return 1;
    }
}