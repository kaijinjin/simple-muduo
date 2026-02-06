#include "TcpConnection.h"
#include "Logger.h"
#include "Channel.h"
#include "Socket.h"
#include "EventLoop.h"


static EventLoop* cehckEventLoopNotNull(EventLoop* eventLoop)
{
    if (eventLoop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection的EventLoop是空\n", __FILE__, __func__, __LINE__);
    }
    return eventLoop;
}

TcpConnection::TcpConnection(EventLoop* eventLoop, 
                            const std::string name, 
                            int sockfd, 
                            const InetAddress& localAddr, 
                            const InetAddress& peerAddr) 
    : eventLoop_(cehckEventLoopNotNull(eventLoop))
    , name_(name)
    , state_(StateE::kConnecting)
    , reading_(true)
    , socket_(std::make_unique<Socket>(sockfd))
    , channel_(std::make_unique<Channel>(eventLoop_))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024)      // 64M
{
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    socket_->setKeepAlive(true);

    LOG_INFO("TCP链接创建，%s fd=%d\n", name.c_str(), sockfd);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TCP链接销毁%s fd=%d 链接状态：%d\n", name_.c_str(), socket_->fd(), state_);
}

void TcpConnection::send(const std::string& buf)
{
    if (state_ == StateE::kConnected)
    {
        if (eventLoop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            /*
            为什么源码中这里使用的this而不是使用shared_from_this？
            1、绝大部分情况下send都是在IOLoop自己的线程中调用
            2、即使是异步调用，也都是先send，然后再shutdown情况下send先入队，shutdown后入队，不会出现链接被析构，然后调用被析构对象的问题
            3、用户违反规则，先shutdown再send，shutdown先入队，send后入队，send被调用的时候，shutdown也只是关闭了写段，send不写数据了而已
            4、严重违反规则的情况下是去tcpServer将该tcp链接移除然后再调用send的情况下才会崩溃
            */
            eventLoop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

void TcpConnection::sendInLoop(const char* data, size_t len)
{
    // 已发送
    ssize_t nwrote = 0;
    // 待发送
    size_t remaining = len;
    bool faultError = false;

    // 之前调用过该TcpConnection的shutdown，服务端对于该链接就是半关闭状态，只读不写
    // 所以对于短链接，服务端发完全部消息就shutdown，对端收完消息也会close发fin，服务端就完全close
    // 对于长连接：需要在应用层确定对端有断开链接的操作（如离开页面，点击退出按钮）才调用shutdown等待对端close
    if (state_ == StateE::kDisconnected)
    {
        LOG_ERROR("该链接已经关闭写端，放弃这次写操作\n");
        return;
    }

    // 没有监听写操作，并且发送缓冲区没有数据要发送，说明这个链接是第一次发送数据，或者说上次发送数据没有数据残留在发送缓冲区
    if (!channel_->isWriting() && ouputBuffer_.readableBytes() == 0)
    {
        nwrote = write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            // 数据发完并且存在回调：发送数据后进行一些操作
            if (remaining == 0 && writeCompleteCallback_)
            {
                // 这里的writeCompleteCallback_中有可能会调用send导致无限递归，所以必须使用queueInLoop
                eventLoop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            // 这个流程的nwrote < 0，需要将nwrote重置为零避免数据污染
            nwrote = 0;
            /*
            EWOULDBLOCK（在某些系统上也叫 EAGAIN）是一个错误码，表示非阻塞操作会阻塞。
            阻塞socket的write()行为
            ssize_t n = write(fd, data, len);  // 假设内核缓冲区已满
            write() 会：
            1. 检查内核缓冲区是否有空间
            2. 如果没有，进程/线程被挂起（进入睡眠状态）
            3. 等待内核缓冲区有空间（可能几毫秒到几秒）
            4. 有空间后，操作系统唤醒进程，写入数据
            5. 返回写入的字节数
            特点：调用者"阻塞"（睡眠）直到操作完成

            非阻塞socket的write()行为（muduo中）
            ssize_t n = write(fd, data, len);  // 假设内核缓冲区已满
            write() 会：
            1. 检查内核缓冲区是否有空间
            2. 如果没有，立即返回 -1
            3. 设置 errno = EWOULDBLOCK/EAGAIN
            4. 调用者保持运行，不会被挂起
            特点：调用者立即得到结果，不会等待
            */
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop, errno:%d\n", errno);
                // EPIPE - Broken Pipe（管道破裂）:对端关闭了连接（close() 或 shutdown(SHUT_RD)），本端却还在写入导致报错
                // ECONNRESET - Connection Reset by Peer（连接被对端重置）:对端进程崩溃（内核发送 RST 包）、对端 socket 被意外关闭
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }

    // 数据一次没发完，还有一部分数据需要保存到缓冲区，需要监听写事件，等待下次发送
    if (!faultError && remaining > 0)
    {
        size_t oldLen = ouputBuffer_.readableBytes();
        // 高水位检查：缓冲区数据已经很多，这次未发送的加上缓冲区的已经大于64M并且存在回调函数，就进行回调操作
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            eventLoop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        // 将剩余数据添加到缓冲区
        ouputBuffer_.append(data + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
}

// 给上层提供如应用层
void TcpConnection::shutdown()
{
    if (state_ == StateE::kConnected)
    {
        // 设置链接状态为取消中
        setState(StateE::kDisconnecting);
        eventLoop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

// 给上层提供如TcpServer
// 链接建立：在TcpServer创建TcpConnection时调用
void TcpConnection::connectEstablished()
{
    setState(StateE::kConnected);
    // 对于底层组件（channel），传递智能指针，channel层通过弱智能指针接收，仅在调用时提升，避免TcpConnection生命周期扩大
    // 也避免上层（应用层）意外将TcpConnection手动释放后，channel依旧访问被释放的对象的问题
    channel_->tie(shared_from_this());
    // 注册到epoll
    channel_->enableReading();
    // 回调：链接创建前的一些操作，在TcpServer创建TcpConnection时设置
    connectionCallback_(shared_from_this());
}

// 给上层提供如TcpServer
// 链接销毁
void TcpConnection::connectDestroyed()
{
    // 通过tcp的状态来避免重复销毁
    if (state_ == StateE::kConnected)
    {
        setState(StateE::kDisconnected);
        // 从epoll下树
        channel_->disableAll();
        // 回调：销毁前的一些操作，在TcpServer创建TcpConnection时设置
        connectionCallback_(shared_from_this());
    }
    // 从poller的map上删除
    channel_->remove();
}

// 给channel提供
// poller => channel::readCallback => TcpConnection::handleRead
void TcpConnection::handleRead(Timestamp reveiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {
        // （TcpConnection由TcpServer的shared_ptr管理）对于上层组件：这里如果传递this指针会导致TcpConnection的生命周期不明确，而使用shared_from_this传递，可以明确的表示TcpConnection由一个shared_ptr管理
        messageCallback_(shared_from_this(), &inputBuffer_, reveiveTime);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead发生错误，errno:%d\n", savedErrno);
        handleError();
    }
}

void TcpConnection::shutdownInLoop()
{
    // 如果channel还在监听写，说明还有数据要发送，就不关闭，写完再关闭
    if (!channel_->isWriting())
    {
        // 关闭写端
        socket_->shutdownWrite();
    }
}

// 给channel提供
// poller => channel::writeCallback => TcpConnection::handleWrite
void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = ouputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0)
        {
            ouputBuffer_.retrieve(n);
            // 发送缓冲区中所有数据都发完了
            if (ouputBuffer_.readableBytes() == 0)
            {
                // 让epoll停止监听写，因为没有数据要发送了
                channel_->disableWriting();
                // 写完后的回调操作，如果有就执行
                if (writeCompleteCallback_)
                {
                    // 这里没有重入风险但是为什么要通过queueInLoop调用？->网络回调层可以包含业务逻辑，但是业务逻辑不应该阻塞网络层，所以不能直接在IOLoop中直接调用回调
                    eventLoop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == StateE::kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite失败errno：%d\n", savedErrno);
        }
    }
    else
    {
        LOG_ERROR("TcpConnection::handleWrite失败，该链接已经关闭写端，fd=%d\n", channel_->fd());
    }
}

// 给channel提供
// 客户端close，服务端接收到了fin：poller => channel::closeCallback => TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose 客户端主动close，服务端收到fin，关闭链接，fd：%d 状态：%d\n", channel_->fd(), state_);
    // 设置Tcp状态为关闭
    setState(StateE::kDisconnected);
    // 将该channel从epoll树上删除，channle还在poller的map上
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    // 回调：销毁前的一些操作，在TcpServer创建TcpConnection时设置，不需要在queueInLoop中调用：即使connectionCallback_->send->sendInLoop.....没有无限递归
    connectionCallback_(connPtr);
    // 回调：关闭tcp链接，TcpServer创建TcpConnection时设置，会绑定TcpConnection::connectDestroyed，同上
    closeCallback_(connPtr);
}

// 给channel提供
// poller => channel::errorCallback => TcpConnection::handleError
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if (getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d\n", name_.c_str(), err);
}