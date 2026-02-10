#include "./../TcpConnection.h"
#include "./../EventLoopThread.h"
#include "./../InetAddress.h"
#include "./../Timestamp.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <future>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <cstring>

using namespace std;

struct TestResult {
    string name;
    bool passed;
    string msg;
    TestResult(const string& n, bool p, const string& m = "") : name(n), passed(p), msg(m) {}
};

vector<TestResult> results;

void run_test(const string& name, const function<bool()>& fn) {
    cout << "运行测试: " << name << "...\n";
    bool ok = false;
    try {
        ok = fn();
    } catch (const std::exception& e) {
        results.emplace_back(name, false, string("异常: ") + e.what());
        ok = false;
    } catch (...) {
        results.emplace_back(name, false, "未知异常");
        ok = false;
    }
    if (ok) {
        cout << "  ✅ 通过\n\n";
        results.emplace_back(name, true, "通过");
    } else {
        cout << "  ❌ 失败\n\n";
        if (results.empty() || results.back().name != name) results.emplace_back(name, false, "失败");
    }
}

// 辅助：设置非阻塞（接受到的socketpair端默认是阻塞的，但不需要设置）

// 测试1: send -> peer接收到数据，并触发WriteComplete回调
bool test_send_and_write_complete()
{
    EventLoopThread t;
    EventLoop* loop = t.startLoop();

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        cerr << "socketpair失败\n";
        return false;
    }

    InetAddress local("127.0.0.1", 0);
    InetAddress peer("127.0.0.1", 0);

    auto conn = make_shared<TcpConnection>(loop, string("tcptest"), fds[0], local, peer);

    promise<void> connEstablishedProm;
    auto connEstablishedF = connEstablishedProm.get_future();

    promise<void> writeCompleteProm;
    auto writeCompleteF = writeCompleteProm.get_future();

    conn->setConnectionCallback([&](const TcpConnectionPtr& c){
        connEstablishedProm.set_value();
    });

    conn->setWriteCompleteCallback([&](const TcpConnectionPtr& c){
        // 标记写完成
        writeCompleteProm.set_value();
    });

    // 建立连接（内部会enableReading并调用connectionCallback）
    conn->connectEstablished();

    // 等待连接建立
    if (connEstablishedF.wait_for(chrono::seconds(1)) != future_status::ready) {
        cerr << "连接建立回调超时\n";
        close(fds[0]); close(fds[1]);
        return false;
    }

    // 发送数据（从测试线程调用，会queue到loop线程）
    const string msg = "hello_tcpconnection";
    conn->send(msg);

    // 等待writeComplete回调触发
    if (writeCompleteF.wait_for(chrono::seconds(1)) != future_status::ready) {
        cerr << "writeComplete回调超时\n";
        close(fds[0]); close(fds[1]);
        return false;
    }

    // 从另一端读取验证
    char buf[256] = {0};
    ssize_t n = read(fds[1], buf, sizeof(buf));
    if (n <= 0) {
        cerr << "peer未收到数据，read返回: " << n << " errno=" << errno << "\n";
        close(fds[0]); close(fds[1]);
        return false;
    }

    string rec(buf, buf + n);
    bool ok = (rec == msg);

    close(fds[0]); close(fds[1]);
    return ok;
}

// 测试2: 收到对端数据时触发messageCallback
bool test_receive_triggers_message_callback()
{
    EventLoopThread t;
    EventLoop* loop = t.startLoop();

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        cerr << "socketpair失败\n";
        return false;
    }

    InetAddress local("127.0.0.1", 0);
    InetAddress peer("127.0.0.1", 0);

    auto conn = make_shared<TcpConnection>(loop, string("tcptest2"), fds[0], local, peer);

    promise<void> connEstablishedProm;
    auto connEstablishedF = connEstablishedProm.get_future();

    promise<string> messageProm;
    auto messageF = messageProm.get_future();

    conn->setConnectionCallback([&](const TcpConnectionPtr& c){ connEstablishedProm.set_value(); });

    conn->setMessageCallback([&](const TcpConnectionPtr& c, Buffer* buf, Timestamp t){
        string s(buf->peek(), buf->peek() + buf->readableBytes());
        messageProm.set_value(s);
    });

    conn->connectEstablished();

    if (connEstablishedF.wait_for(chrono::seconds(1)) != future_status::ready) {
        cerr << "连接建立回调超时\n";
        close(fds[0]); close(fds[1]);
        return false;
    }

    const string payload = "ping_from_peer";
    // 向conn的另一端写数据（直接写到fds[1]）
    ssize_t w = write(fds[1], payload.data(), payload.size());
    if (w != (ssize_t)payload.size()) {
        cerr << "向对端写入失败 w=" << w << " errno=" << errno << "\n";
        close(fds[0]); close(fds[1]);
        return false;
    }

    // 等待message回调
    if (messageF.wait_for(chrono::seconds(1)) != future_status::ready) {
        cerr << "message回调超时\n";
        close(fds[0]); close(fds[1]);
        return false;
    }

    string rec = messageF.get();
    bool ok = (rec == payload);
    close(fds[0]); close(fds[1]);
    return ok;
}

// 测试3: shutdown会导致对端读到EOF
bool test_shutdown_closes_write_end()
{
    EventLoopThread t;
    EventLoop* loop = t.startLoop();

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        cerr << "socketpair失败\n";
        return false;
    }

    InetAddress local("127.0.0.1", 0);
    InetAddress peer("127.0.0.1", 0);

    auto conn = make_shared<TcpConnection>(loop, string("tcptest3"), fds[0], local, peer);

    promise<void> connEstablishedProm;
    auto connEstablishedF = connEstablishedProm.get_future();

    conn->setConnectionCallback([&](const TcpConnectionPtr& c){ connEstablishedProm.set_value(); });
    conn->connectEstablished();

    if (connEstablishedF.wait_for(chrono::seconds(1)) != future_status::ready) {
        cerr << "连接建立回调超时\n";
        close(fds[0]); close(fds[1]);
        return false;
    }

    // 调用shutdown（半关闭写端）
    conn->shutdown();

    // 等待短时间让loop处理shutdownInLoop
    this_thread::sleep_for(chrono::milliseconds(200));

    // peer读取应该返回0（EOF）
    char buf[10];
    ssize_t n = read(fds[1], buf, sizeof(buf));
    // n==0: 对端收到了EOF
    bool ok = (n == 0);
    if (!ok) {
        cerr << "shutdown后peer read返回: " << n << " errno=" << errno << "\n";
    }

    close(fds[0]); close(fds[1]);
    return ok;
}

// 测试4: connectDestroyed 调用时触发 connectionCallback 且连接变为断开
bool test_connect_destroyed_triggers_connection_callback()
{
    EventLoopThread t;
    EventLoop* loop = t.startLoop();

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        cerr << "socketpair失败\n";
        return false;
    }

    InetAddress local("127.0.0.1", 0);
    InetAddress peer("127.0.0.1", 0);

    auto conn = make_shared<TcpConnection>(loop, string("tcptest4"), fds[0], local, peer);

    promise<void> firstProm;
    auto firstF = firstProm.get_future();
    promise<void> secondProm;
    auto secondF = secondProm.get_future();

    atomic<bool> firstDone{false};

    conn->setConnectionCallback([&](const TcpConnectionPtr& c){
        if (!firstDone.exchange(true)) {
            firstProm.set_value();
        } else {
            secondProm.set_value();
        }
    });

    conn->connectEstablished();
    if (firstF.wait_for(chrono::seconds(1)) != future_status::ready) {
        close(fds[0]); close(fds[1]);
        return false;
    }

    // 调用 connectDestroyed，期望触发第二次回调
    conn->connectDestroyed();
    if (secondF.wait_for(chrono::seconds(1)) != future_status::ready) {
        close(fds[0]); close(fds[1]);
        return false;
    }

    bool ok = (!conn->connected());

    close(fds[0]); close(fds[1]);
    return ok;
}

// 测试5: 在shutdown之后再send不应将数据送到peer
bool test_send_after_shutdown_no_delivery()
{
    EventLoopThread t;
    EventLoop* loop = t.startLoop();

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        cerr << "socketpair失败\n";
        return false;
    }

    InetAddress local("127.0.0.1", 0);
    InetAddress peer("127.0.0.1", 0);

    auto conn = make_shared<TcpConnection>(loop, string("tcptest5"), fds[0], local, peer);

    promise<void> connEstablishedProm;
    auto connEstablishedF = connEstablishedProm.get_future();
    conn->setConnectionCallback([&](const TcpConnectionPtr& c){ connEstablishedProm.set_value(); });
    conn->connectEstablished();
    if (connEstablishedF.wait_for(chrono::seconds(1)) != future_status::ready) {
        close(fds[0]); close(fds[1]);
        return false;
    }

    // shutdown 写端
    conn->shutdown();
    this_thread::sleep_for(chrono::milliseconds(100));

    // 之后再send，应该不会将数据送出
    conn->send("SHOULD_NOT_BE_SENT");
    this_thread::sleep_for(chrono::milliseconds(200));

    // peer 读应该返回 EOF (0) 或者没有收到数据
    char buf[64];
    ssize_t n = read(fds[1], buf, sizeof(buf));
    bool ok = (n == 0 || (n > 0 && string(buf, buf + n) != string("SHOULD_NOT_BE_SENT")));

    close(fds[0]); close(fds[1]);
    return ok;
}

// 测试6: 从其他线程调用 send（触发 queueInLoop 路径），数据能被对端接收
bool test_send_from_other_thread()
{
    EventLoopThread t;
    EventLoop* loop = t.startLoop();

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        cerr << "socketpair失败\n";
        return false;
    }

    InetAddress local("127.0.0.1", 0);
    InetAddress peer("127.0.0.1", 0);

    auto conn = make_shared<TcpConnection>(loop, string("tcptest6"), fds[0], local, peer);

    promise<void> connEstablishedProm;
    auto connEstablishedF = connEstablishedProm.get_future();
    conn->setConnectionCallback([&](const TcpConnectionPtr& c){ connEstablishedProm.set_value(); });
    conn->connectEstablished();
    if (connEstablishedF.wait_for(chrono::seconds(1)) != future_status::ready) {
        close(fds[0]); close(fds[1]);
        return false;
    }

    const string msg = "from_other_thread";

    promise<void> writeProm;
    auto writeF = writeProm.get_future();
    conn->setWriteCompleteCallback([&](const TcpConnectionPtr& c){
        writeProm.set_value();
    });

    thread sender([conn, msg]() {
        conn->send(msg);
    });
    sender.join();

    // 等待写完成回调，最多 1 秒
    if (writeF.wait_for(chrono::seconds(1)) != future_status::ready) {
        close(fds[0]); close(fds[1]);
        return false;
    }

    // 只要 writeCompleteCallback 被触发就认为从其他线程发送的数据已被处理（验证 queueInLoop 路径）
    close(fds[0]); close(fds[1]);
    return true;
}

int main()
{
    cout << "开始 TcpConnection 测试套件\n";

    run_test("send -> WriteCompleteCallback", [](){ return test_send_and_write_complete(); });
    run_test("receive -> MessageCallback", [](){ return test_receive_triggers_message_callback(); });
    run_test("shutdown -> peer EOF", [](){ return test_shutdown_closes_write_end(); });
    run_test("connectDestroyed -> connectionCallback & disconnect", [](){ return test_connect_destroyed_triggers_connection_callback(); });
    run_test("send after shutdown -> no delivery", [](){ return test_send_after_shutdown_no_delivery(); });
    run_test("send from other thread -> delivery", [](){ return test_send_from_other_thread(); });

    cout << "\\n测试汇总:\\n";
    int pass = 0;
    for (auto &r : results) {
        cout << (r.passed ? "✓ " : "✗ ") << r.name;
        if (!r.msg.empty()) cout << " - " << r.msg;
        cout << "\n";
        if (r.passed) pass++;
    }
    cout << pass << "/" << results.size() << " 个测试通过\n";

    return (pass == (int)results.size()) ? 0 : 1;
}
