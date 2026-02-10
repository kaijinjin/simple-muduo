#include "./../TcpServer.h"
#include "./../TcpConnection.h"
#include "./../Logger.h"
#include "./../Timestamp.h"
#include "./../EventLoop.h"

class EchoSever
{
public:
    EchoSever(EventLoop* eventLoop, const InetAddress& address, const std::string& name)
        : server_(eventLoop, address, name)
        , loop_(eventLoop)
        {
            server_.setConnectionCallback(std::bind(&EchoSever::onConnection, this, std::placeholders::_1));
            server_.setMessageCallback(std::bind(&EchoSever::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            server_.setThreadNum(3);
        }

    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            LOG_INFO("有新链接：%s\n", conn->getPeerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("链接销毁：%s\n", conn->getPeerAddress().toIpPort().c_str());
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp now)
    {
        std::string msg(buf->retrieveAllAsString());
        conn->send(msg);
        // conn->shutdown();
    }

    EventLoop *loop_;
    TcpServer server_;
};


int main()
{
    EventLoop loop;
    InetAddress addr("127.0.0.1", 8080);
    EchoSever echoServer(&loop, addr, "echo-1");
    echoServer.start();
    loop.loop();
    return 0;
}