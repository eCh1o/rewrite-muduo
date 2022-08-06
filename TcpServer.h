#pragma once

/*
用户使用muduo库编写服务器
*/
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callback.h"

#include <functional>
#include <atomic>
#include <unordered_map>

//对外的服务器编程使用的类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    enum Option
    {
        KNoReusePort,
        KReusePort,
    };
    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const std::string &nameArg,
              Option option = KNoReusePort);
    ~TcpServer();
    void setThreadInitcallback(const ThreadInitCallback &cb) { threadinitcallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectioncallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messagecallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writecompletecallback_ = cb; }
    //设置底层subloop的个数
    void setThreadNum(int numThreads);
    //开始服务器监听
    void start();

private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionptr &conn);
    void removeConnectionInLoop(const TcpConnectionptr &conn);
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionptr>;
    EventLoop *loop_; // baseloop用户定义的loop
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;              //运行在mainloop，监听新连接事件
    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread

    ConnectionCallback connectioncallback_;       //有新连接时的回调
    MessageCallback messagecallback_;             //有读写消息时的回调
    WriteCompleteCallback writecompletecallback_; //消息发送完成以后的回调
    ThreadInitCallback threadinitcallback_;       // loop线程初始化的回调
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_; //保存所有的连接
};