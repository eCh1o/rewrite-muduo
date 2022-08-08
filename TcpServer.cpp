#include "TcpServer.h"
#include "logger.h"

#include <strings.h>
static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const std::string &nameArg,
                     Option option)
    : loop_(CheckLoopNotNull(loop)),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, option == KReusePort)),
      threadPool_(new EventLoopThreadPool(loop, name_)),
      connectioncallback_(),
      messagecallback_(),
      nextConnId_(1)
{
    //当有新用户连接时，会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                                  std::placeholders::_1,
                                                  std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for(auto &item : connections_)
    {
        TcpConnectionptr conn(item.second);
        item.second.reset();
        //销毁连接
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroy,conn));
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    if (started_++ == 0) //防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadinitcallback_); //启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

//有一个新客户端的连接，acceptror会执行这个回调
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    //轮询算法，选择一个subloop，来管理channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    //通过sockfd获取其绑定的本机的IP地址和端口信息
    sockaddr_in local;
    bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    //根据连接成功的sockfd，创建TcpConnection连接对象
    TcpConnectionptr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;
    //下面的回调都是用户设置给TcpServer=>TcpConnection=>channel=>poller=>通知channel调用回调
    conn->setConnectionCallback(connectioncallback_);
    conn->setMessageCallback(messagecallback_);
    conn->setWriteCompleteCallback(writecompletecallback_);
    //设置了如何关闭连接的回调
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
    //直接调用
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionptr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop,this,conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionptr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s \n",
    name_.c_str(),conn->name().c_str());
    connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroy, conn));
}