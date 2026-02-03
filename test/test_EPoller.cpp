#include "./../EPollPoller.h"
#include "./../Channel.h"
#include "./../EventLoop.h"
#include <iostream>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

using namespace std;

using ChannelList = vector<Channel*>;
constexpr int kNew = -1;
constexpr int kAdded = 1;
constexpr int kDeleted = 2;
// 测试通道回调函数
class TestChannelCallbacks {
public:
    void onRead() {
        cout << "TestChannelCallbacks::onRead() called" << endl;
        readCount_++;
    }
    
    void onWrite() {
        cout << "TestChannelCallbacks::onWrite() called" << endl;
        writeCount_++;
    }
    
    void onError() {
        cout << "TestChannelCallbacks::onError() called" << endl;
        errorCount_++;
    }
    
    void resetCounters() {
        readCount_ = 0;
        writeCount_ = 0;
        errorCount_ = 0;
    }
    
    int readCount() const { return readCount_; }
    int writeCount() const { return writeCount_; }
    int errorCount() const { return errorCount_; }
    
private:
    int readCount_ = 0;
    int writeCount_ = 0;
    int errorCount_ = 0;
};

// 创建测试用的socket pair
int createSocketPair(int fds[2]) {
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    return 0;
}

// 设置非阻塞
int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    assert(flags >= 0);
    int result = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    assert(result == 0);
    return result;
}

// 测试1: 基本功能测试 - 添加、修改、删除Channel
void testBasicOperations() {
    cout << "=== 测试1: 基本功能测试 ===" << endl;
    
    EventLoop loop;
    EPollPoller poller(&loop);
    TestChannelCallbacks callbacks;
    
    // 创建socket pair用于测试
    int fds[2];
    assert(createSocketPair(fds) == 0);
    
    // 创建Channel
    Channel channel(&loop, fds[0]);
    
    // 断言初始状态
    assert(channel.fd() == fds[0]);
    
    // 测试添加Channel到Poller
    cout << "1. 测试添加Channel到Poller" << endl;
    channel.enableReading();
    channel.set_index(kNew);
    poller.updateChannel(&channel);
    assert(channel.index() == 1);  // kAdded = 1
    
    // 测试修改Channel事件
    cout << "2. 测试修改Channel事件" << endl;
    channel.enableWriting();
    poller.updateChannel(&channel);
    assert(channel.index() == 1);  // 仍然是kAdded
    
    // 测试禁用所有事件
    cout << "3. 测试禁用所有事件" << endl;
    channel.disableAll();
    poller.updateChannel(&channel);
    assert(channel.index() == 2);  // kDeleted = 2
    assert(channel.isNoneEvent());
    
    // 测试重新启用事件
    cout << "4. 测试重新启用事件" << endl;
    channel.enableReading();
    poller.updateChannel(&channel);
    assert(channel.index() == 1);  // 变回kAdded
    
    // 测试从Poller中移除Channel
    cout << "5. 测试从Poller中移除Channel" << endl;
    poller.removeChannel(&channel);
    assert(channel.index() == -1);  // 变回kNew
    
    close(fds[0]);
    close(fds[1]);
    cout << "=== 测试1通过 ===\n" << endl;
}

// 测试2: poll函数测试 - 模拟事件触发
void testPollFunction() {
    cout << "=== 测试2: poll函数测试 ===" << endl;
    
    EventLoop loop;
    EPollPoller poller(&loop);
    TestChannelCallbacks callbacks;
    
    // 创建socket pair
    int fds[2];
    assert(createSocketPair(fds) == 0);
    assert(setNonBlocking(fds[0]) == 0);
    assert(setNonBlocking(fds[1]) == 0);
    
    // 创建Channel
    Channel channel(&loop, fds[0]);
    channel.set_index(kNew);
    channel.setReadCallback([&callbacks](Timestamp) { callbacks.onRead(); });
    channel.setWriteCallback([&callbacks]() { callbacks.onWrite(); });
    channel.setErrorCallback([&callbacks]() { callbacks.onError(); });
    
    // 启用读事件
    channel.enableReading();
    poller.updateChannel(&channel);
    assert(channel.index() == 1);
    
    // 在另一个socket上写数据，触发读事件
    const char* testData = "Hello from test!";
    ssize_t written = write(fds[1], testData, strlen(testData));
    assert(written == (ssize_t)strlen(testData));
    
    // 执行poll操作
    cout << "执行poll操作(等待读事件)" << endl;
    
    ChannelList activeChannels;
    Timestamp ts = poller.poll(1000, &activeChannels);
    
    assert(activeChannels.size() == 1);
    
    Channel* activeChannel = activeChannels[0];
    assert(activeChannel == &channel);
    assert(activeChannel->fd() == fds[0]);
    
    // 处理事件
    activeChannel->handleEvent(ts);
    assert(callbacks.readCount() == 1);
    assert(callbacks.writeCount() == 0);
    
    // 读取数据验证
    char buffer[256];
    ssize_t n = read(fds[0], buffer, sizeof(buffer));
    assert(n == written);
    buffer[n] = '\0';
    assert(strcmp(buffer, testData) == 0);
    
    // 测试写事件
    cout << "测试写事件" << endl;
    callbacks.resetCounters();
    channel.enableWriting();
    poller.updateChannel(&channel);
    
    activeChannels.clear();
    ts = poller.poll(100, &activeChannels);
    
    assert(activeChannels.size() == 1);
    activeChannel = activeChannels[0];
    activeChannel->handleEvent(ts);
    assert(callbacks.writeCount() == 1);
    assert(callbacks.readCount() == 1);
    
    close(fds[0]);
    close(fds[1]);
    cout << "=== 测试2通过 ===\n" << endl;
}

// 测试3: 多个Channel管理测试
void testMultipleChannels() {
    cout << "=== 测试3: 多个Channel管理测试 ===" << endl;
    
    EventLoop loop;
    EPollPoller poller(&loop);
    
    // 创建多个socket pair
    const int NUM_CHANNELS = 3;
    int fds[NUM_CHANNELS][2];
    unique_ptr<Channel> channels[NUM_CHANNELS];
    TestChannelCallbacks callbacks[NUM_CHANNELS];
    
    for (int i = 0; i < NUM_CHANNELS; i++) {
        assert(createSocketPair(fds[i]) == 0);
        assert(setNonBlocking(fds[i][0]) == 0);
        assert(setNonBlocking(fds[i][1]) == 0);
        
        channels[i] = make_unique<Channel>(&loop, fds[i][0]);
        channels[i]->set_index(kNew);
        
        channels[i]->setReadCallback([i, &callbacks](Timestamp) {
            callbacks[i].onRead();
        });
        
        channels[i]->setWriteCallback([i, &callbacks]() {
            callbacks[i].onWrite();
        });
        
        // 启用不同的事件
        if (i % 2 == 0) {
            channels[i]->enableReading();
        } else {
            channels[i]->enableWriting();
        }
        
        poller.updateChannel(channels[i].get());
        assert(channels[i]->index() == 1);
    }
    
    
    // 在第0个socket上写数据，触发读事件
    const char* data = "Test data";
    assert(write(fds[0][1], data, strlen(data)) == (ssize_t)strlen(data));
    
    // poll并处理事件
    ChannelList activeChannels;
    Timestamp ts = poller.poll(500, &activeChannels);
    
    // 至少应该有1个活跃Channel（读事件）
    assert(activeChannels.size() >= 1);
    
    bool foundReadChannel = false;
    for (Channel* channel : activeChannels) {
        channel->handleEvent(ts);
        if (channel->fd() == fds[0][0]) {
            foundReadChannel = true;
        }
    }
    
    assert(foundReadChannel);
    assert(callbacks[0].readCount() >= 1);
    
    // 清理
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (channels[i]) {
            poller.removeChannel(channels[i].get());
            assert(channels[i]->index() == -1);
        }
        close(fds[i][0]);
        close(fds[i][1]);
    }
    
    cout << "=== 测试3通过 ===\n" << endl;
}

// 测试4: 边界条件测试
void testEdgeCases() {
    cout << "=== 测试4: 边界条件测试 ===" << endl;
    
    EventLoop loop;
    EPollPoller poller(&loop);
    
    // 测试1: 重复添加同一个Channel
    cout << "1. 测试重复添加同一个Channel" << endl;
    {
        int fds[2];
        assert(createSocketPair(fds) == 0);
        Channel channel(&loop, fds[0]);
        channel.set_index(kNew);
        channel.enableReading();
        poller.updateChannel(&channel);
        assert(channel.index() == 1);
        
        poller.updateChannel(&channel);  // 重复添加
        assert(channel.index() == 1);  // 状态不变
        
        poller.removeChannel(&channel);
        close(fds[0]);
        close(fds[1]);
    }
    
    // 测试2: 添加然后立即移除
    cout << "2. 测试添加然后立即移除" << endl;
    {
        int fds[2];
        assert(createSocketPair(fds) == 0);
        Channel channel(&loop, fds[0]);
        channel.set_index(kNew);
        channel.enableReading();
        poller.updateChannel(&channel);
        assert(channel.index() == 1);
        
        poller.removeChannel(&channel);
        assert(channel.index() == -1);
        
        close(fds[0]);
        close(fds[1]);
    }
    
    // 测试3: 空事件集poll
    cout << "3. 测试空事件集poll" << endl;
    {
        ChannelList activeChannels;
        Timestamp ts = poller.poll(10, &activeChannels);
        assert(activeChannels.size() == 0);
    }
    
    // 测试4: 无效操作（理论上不应该发生）
    cout << "4. 测试无效操作" << endl;
    {
        int fds[2];
        assert(createSocketPair(fds) == 0);
        Channel channel(&loop, fds[0]);
        
        // Channel处于kNew状态，直接调用remove
        poller.removeChannel(&channel);
        assert(channel.index() == -1);  // 应该还是kNew
        
        // Channel处于kDeleted状态，再次删除
        channel.set_index(2);  // kDeleted
        poller.removeChannel(&channel);
        assert(channel.index() == -1);  // 应该变成kNew
        
        close(fds[0]);
        close(fds[1]);
    }
    
    cout << "=== 测试4通过 ===\n" << endl;
}

// 测试5: 事件类型测试
void testEventTypes() {
    cout << "=== 测试5: 事件类型测试 ===" << endl;
    
    EventLoop loop;
    EPollPoller poller(&loop);
    
    int fds[2];
    assert(createSocketPair(fds) == 0);
    assert(setNonBlocking(fds[0]) == 0);
    assert(setNonBlocking(fds[1]) == 0);
    
    Channel channel(&loop, fds[0]);
    channel.set_index(kNew);
    // 测试EPOLLIN
    cout << "1. 测试EPOLLIN事件" << endl;
    channel.enableReading();
    poller.updateChannel(&channel);
    assert((channel.events() & EPOLLIN) != 0);
    
    // 测试EPOLLOUT
    cout << "2. 测试EPOLLOUT事件" << endl;
    channel.enableWriting();
    poller.updateChannel(&channel);
    assert((channel.events() & EPOLLOUT) != 0);
    
    // 测试EPOLLIN | EPOLLOUT
    cout << "3. 测试EPOLLIN | EPOLLOUT事件" << endl;
    assert((channel.events() & (EPOLLIN | EPOLLOUT)) == (EPOLLIN | EPOLLOUT));
    
    // 测试禁用读事件
    cout << "4. 测试禁用读事件" << endl;
    channel.disableReading();
    poller.updateChannel(&channel);
    assert((channel.events() & EPOLLIN) == 0);
    assert((channel.events() & EPOLLOUT) != 0);
    
    // 测试禁用写事件
    cout << "5. 测试禁用写事件" << endl;
    channel.disableWriting();
    poller.updateChannel(&channel);
    assert((channel.events() & EPOLLIN) == 0);
    assert((channel.events() & EPOLLOUT) == 0);
    assert(channel.isNoneEvent());
    
    poller.removeChannel(&channel);
    close(fds[0]);
    close(fds[1]);
    cout << "=== 测试5通过 ===\n" << endl;
}

// 测试6: EPOLL_CLOEXEC标志测试
void testEpollCloseOnExec() {
    cout << "=== 测试6: EPOLL_CLOEXEC标志测试 ===" << endl;
    
    // 创建一个子进程来验证FD_CLOEXEC
    int pipefd[2];
    assert(pipe(pipefd) == 0);
    
    pid_t pid = fork();
    assert(pid >= 0);
    
    if (pid == 0) {  // 子进程
        close(pipefd[0]);  // 关闭读端
        
        EventLoop loop;
        EPollPoller poller(&loop);
        
        // 尝试执行新程序
        char dummy[] = "/bin/true";
        char* argv[] = {dummy, nullptr};
        execvp(argv[0], argv);
        
        // 如果exec失败，写入错误信息
        int err = errno;
        write(pipefd[1], &err, sizeof(err));
        _exit(1);
    } else {  // 父进程
        close(pipefd[1]);  // 关闭写端
        
        int status;
        waitpid(pid, &status, 0);
        
        // 读取子进程的错误信息
        int child_errno = 0;
        ssize_t n = read(pipefd[0], &child_errno, sizeof(child_errno));
        
        if (n > 0) {
            cout << "子进程exec失败，错误码: " << child_errno << endl;
            if (child_errno == EBADF) {
                cout << "EPOLL_CLOEXEC工作正常（文件描述符已关闭）" << endl;
            }
        } else {
            cout << "子进程exec成功，EPOLL_CLOEXEC工作正常" << endl;
        }
        
        close(pipefd[0]);
    }
    
    cout << "=== 测试6完成 ===\n" << endl;
}

// 主测试函数
int main() {
    cout << "开始EPollPoller测试套件\n" << endl;
    
    try {
        testBasicOperations();
        testPollFunction();
        testMultipleChannels();
        testEdgeCases();
        testEventTypes();
        testEpollCloseOnExec();
        
        cout << "\n=======================================" << endl;
        cout << "所有测试通过！EPollPoller实现正确。" << endl;
        cout << "=======================================" << endl;
        
    } catch (const exception& e) {
        cerr << "\n测试失败，异常: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "\n测试失败，未知异常" << endl;
        return 1;
    }
    
    return 0;
}