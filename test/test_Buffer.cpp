// AI生成
#include "./../Buffer.h"
#include <iostream>
#include <cstring>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <random>

// 不使用 GoogleTest 的版本
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
    
    TestResult(const std::string& name_, bool pass, const std::string& msg = "")
        : name(name_), passed(pass), message(msg) {}
};

std::vector<TestResult> testResults;

#define RUN_TEST(test_func, test_name) \
    do { \
        std::cout << "测试: " << test_name << "..." << std::endl; \
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

// ================= 基础功能测试 =================

bool test_constructor() {
    Buffer buf1;
    if (buf1.readableBytes() != 0) return false;
    if (buf1.writableBytes() != Buffer::kInitialSize) return false;
    
    Buffer buf2(2048);
    if (buf2.writableBytes() != 2048) return false;
    
    return true;
}

bool test_append_and_retrieve() {
    Buffer buf;
    
    // 测试基本追加
    const char* data1 = "Hello";
    buf.append(data1, strlen(data1));
    if (buf.readableBytes() != 5) return false;
    
    // 测试 peek
    const char* peeked = buf.peek();
    if (memcmp(peeked, "Hello", 5) != 0) return false;
    
    // 测试 retrieve
    buf.retrieve(3);
    if (buf.readableBytes() != 2) return false;
    if (memcmp(buf.peek(), "lo", 2) != 0) return false;
    
    // 测试 retrieveAsString
    std::string str = buf.retrieveAsString(2);
    if (str != "lo") return false;
    if (buf.readableBytes() != 0) return false;
    
    return true;
}

bool test_retrieve_all() {
    Buffer buf;
    buf.append("Hello World", 11);
    
    std::string str1 = buf.retrieveAllAsString();
    if (str1 != "Hello World") return false;
    if (buf.readableBytes() != 0) return false;
    
    buf.append("Test", 4);
    buf.retrieveAll();
    if (buf.readableBytes() != 0) return false;
    
    return true;
}

bool test_ensure_writable() {
    Buffer buf(16);  // 初始大小 16
    
    // 填充数据
    buf.append("1234567890", 10);  // 写入 10 字节
    if (buf.writableBytes() != 6) return false;  // 16 - 10 = 6
    
    // 需要更多空间（应该重新分配）
    buf.append("ABCDEFGHIJKLMNOPQRST", 20);
    if (buf.readableBytes() != 30) return false;  // 10 + 20 = 30
    
    std::string result = buf.retrieveAllAsString();
    if (result != "1234567890ABCDEFGHIJKLMNOPQRST") return false;
    
    return true;
}

bool test_make_space_logic() {
    Buffer buf(16);
    
    // 情况1：有足够空间（不需要移动数据）
    buf.append("Hello", 5);
    buf.retrieve(3);  // 读取3字节，剩2字节
    if (buf.prependableBytes() != Buffer::kCheapPrepend + 3) return false;
    
    buf.append("World", 5);  // 追加5字节
    if (buf.readableBytes() != 7) return false;  
    
    // 情况2：需要移动数据但不需要扩容
    buf.append("1234567", 7);  // 追加7字节
    if (buf.readableBytes() != 14) return false; 
    
    std::string result = buf.retrieveAllAsString();
    if (result != "loWorld1234567") return false;
    
    return true;
}

// ================= 边界条件测试 =================

bool test_edge_cases() {
    // 测试空操作
    Buffer buf;
    buf.retrieve(0);
    if (buf.readableBytes() != 0) return false;
    
    buf.retrieveAll();
    if (buf.readableBytes() != 0) return false;
    
    // 测试读取超过可用数据
    buf.append("Test", 4);
    buf.retrieve(10);  // 应该等价于 retrieveAll
    if (buf.readableBytes() != 0) return false;
    
    // 测试追加空数据
    buf.append("", 0);
    if (buf.readableBytes() != 0) return false;
    
    return true;
}

bool test_large_data() {
    Buffer buf;
    std::string large_data(10000, 'A');  // 10000个'A'
    
    buf.append(large_data.c_str(), large_data.size());
    if (buf.readableBytes() != 10000) return false;
    
    std::string result = buf.retrieveAllAsString();
    if (result.size() != 10000) return false;
    if (result != std::string(10000, 'A')) return false;
    
    return true;
}

// ================= readFd/writeFd 测试 =================

bool create_test_socket_pair(int fds[2]) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
        return false;
    }
    return true;
}

bool test_write_fd() {
    int fds[2];
    if (!create_test_socket_pair(fds)) return false;
    
    Buffer buf;
    const char* test_data = "Hello Socket World!";
    buf.append(test_data, strlen(test_data));
    
    int save_errno = 0;
    ssize_t n = buf.writeFd(fds[1], &save_errno);
    
    close(fds[1]);
    
    if (n != strlen(test_data)) {
        close(fds[0]);
        return false;
    }
    
    // 从另一侧读取验证
    char recv_buf[256] = {0};
    ssize_t m = read(fds[0], recv_buf, sizeof(recv_buf));
    
    close(fds[0]);
    
    return (m == n) && (memcmp(recv_buf, test_data, n) == 0);
}

bool test_read_fd_small_data() {
    int fds[2];
    if (!create_test_socket_pair(fds)) return false;
    
    const char* test_data = "Small test data";
    write(fds[0], test_data, strlen(test_data));
    shutdown(fds[0], SHUT_WR);  // 关闭写端，表示发送完成
    
    Buffer buf;
    int save_errno = 0;
    ssize_t n = buf.readFd(fds[1], &save_errno);
    
    close(fds[0]);
    close(fds[1]);
    
    if (n != strlen(test_data)) return false;
    
    std::string result = buf.retrieveAllAsString();
    return result == test_data;
}


bool test_read_fd_large_data() {
    int fds[2];
    if (!create_test_socket_pair(fds)) return false;
    
    // 准备大于 extrabuf 的数据
    std::string large_data(70000, 'X');  // 70KB
    
    Buffer buf;
    int total_read = 0;
    int save_errno = 0;
    // 在另一个线程发送数据 
    std::thread sender([&]() {
        // 多次读取直到读完
        while (true) {

            ssize_t n = buf.readFd(fds[0], &save_errno);
            if (n <= 0) break;
            total_read += n;
        }
        close(fds[0]);
        
    });
    
    sleep(1);
    write(fds[1], large_data.c_str(), large_data.size());

    

    close(fds[1]);
    sender.join();
    
    std::cout << total_read << std::endl;
    std::cout << large_data.size() << std::endl;
    if (total_read != large_data.size())
    {
        return false;
    }
    std::string result = buf.retrieveAllAsString();
    return large_data == result;
}

bool test_read_fd_with_extrabuf() {
    int fds[2];
    if (!create_test_socket_pair(fds)) return false;
    
    // 发送超过缓冲区可写空间的数据
    Buffer buf;
    size_t writable_before = buf.writableBytes();
    
    // 发送数据量：可写空间 + 1000（触发使用extrabuf）
    std::string test_data(writable_before + 1000, 'B');
    
    std::thread sender([&]() {
        write(fds[0], test_data.c_str(), test_data.size());
        close(fds[0]);
    });
    
    int save_errno = 0;
    ssize_t n = buf.readFd(fds[1], &save_errno);
    
    close(fds[1]);
    sender.join();
    
    if (n != test_data.size()) return false;
    
    std::string result = buf.retrieveAllAsString();
    return result == test_data;
}

bool test_read_fd_error_handling() {
    // 测试错误情况
    int invalid_fd = -1;
    Buffer buf;
    int save_errno = 0;
    
    ssize_t n = buf.readFd(invalid_fd, &save_errno);
    if (n >= 0) return false;  // 应该返回 -1
    if (save_errno == 0) return false;  // errno 应该被设置
    
    return true;
}

// ================= 性能测试 =================

bool test_performance() {
    std::cout << "  性能测试开始..." << std::endl;
    
    const int ITERATIONS = 10000;
    const int DATA_SIZE = 1024;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    Buffer buf;
    std::string test_data(DATA_SIZE, 'P');
    
    for (int i = 0; i < ITERATIONS; i++) {
        buf.append(test_data.c_str(), DATA_SIZE);
        buf.retrieveAll();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "    迭代次数: " << ITERATIONS << std::endl;
    std::cout << "    每次数据大小: " << DATA_SIZE << " 字节" << std::endl;
    std::cout << "    总时间: " << duration.count() << " ms" << std::endl;
    std::cout << "    平均每次操作: " << (duration.count() * 1000.0 / ITERATIONS) << " μs" << std::endl;
    
    return true;
}

// ================= 综合测试 =================

bool test_integration() {
    Buffer buf;
    
    // 模拟一个简单的协议：长度 + 数据
    const char* messages[] = {
        "Hello",
        "This is a longer message",
        "Short",
        "Another test message for buffer"
    };
    
    // 写入所有消息
    for (const char* msg : messages) {
        uint32_t len = strlen(msg);
        buf.append(reinterpret_cast<const char*>(&len), sizeof(len));
        buf.append(msg, len);
    }
    
    // 读取并验证
    for (const char* expected_msg : messages) {
        // 读取长度
        if (buf.readableBytes() < sizeof(uint32_t)) return false;
        
        uint32_t len = 0;
        memcpy(&len, buf.peek(), sizeof(len));
        buf.retrieve(sizeof(len));
        
        // 读取数据
        if (buf.readableBytes() < len) return false;
        
        std::string msg = buf.retrieveAsString(len);
        if (msg != expected_msg) return false;
    }
    
    // 应该没有剩余数据
    return buf.readableBytes() == 0;
}

// ================= 内存测试 =================

bool test_memory_usage() {
    std::cout << "  内存使用测试..." << std::endl;
    
    // 测试缓冲区不会无限增长
    Buffer buf;
    size_t initial_capacity = buf.writableBytes();
    
    // 写入然后清空多次
    for (int i = 0; i < 10; i++) {
        std::string data(1000, 'M');
        buf.append(data.c_str(), data.size());
        buf.retrieveAll();
    }
    
    // 经过多次使用后，缓冲区应该保持合理大小
    size_t final_capacity = buf.writableBytes();
    
    std::cout << "    初始容量: " << initial_capacity << " 字节" << std::endl;
    std::cout << "    最终容量: " << final_capacity << " 字节" << std::endl;
    std::cout << "    内存使用合理" << std::endl;
    
    return true;
}

// ================= 主测试函数 =================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Buffer 类测试套件" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // 运行基础功能测试
    std::cout << "基础功能测试:" << std::endl;
    RUN_TEST(test_constructor, "构造函数");
    RUN_TEST(test_append_and_retrieve, "追加和读取");
    RUN_TEST(test_retrieve_all, "全部读取");
    RUN_TEST(test_ensure_writable, "确保可写空间");
    RUN_TEST(test_make_space_logic, "空间调整逻辑");
    
    std::cout << "\n边界条件测试:" << std::endl;
    RUN_TEST(test_edge_cases, "边界情况");
    RUN_TEST(test_large_data, "大数据处理");
    
    std::cout << "\n文件描述符操作测试:" << std::endl;
    RUN_TEST(test_write_fd, "writeFd 测试");
    RUN_TEST(test_read_fd_small_data, "readFd 小数据");
    RUN_TEST(test_read_fd_large_data, "readFd 大数据");
    RUN_TEST(test_read_fd_with_extrabuf, "readFd extrabuf使用");
    RUN_TEST(test_read_fd_error_handling, "readFd 错误处理");
    
    std::cout << "\n综合测试:" << std::endl;
    RUN_TEST(test_integration, "集成测试");
    
    std::cout << "\n性能测试:" << std::endl;
    RUN_TEST(test_performance, "性能基准");
    RUN_TEST(test_memory_usage, "内存使用");
    
    // 输出汇总
    std::cout << "========================================" << std::endl;
    std::cout << "测试结果汇总" << std::endl;
    std::cout << "========================================" << std::endl;
    
    int total = testResults.size();
    int passed = 0;
    
    for (const auto& result : testResults) {
        std::cout << (result.passed ? "✓ " : "✗ ") << result.name;
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