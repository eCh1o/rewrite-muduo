#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callback.h"
#include "Buffer.h"
#include "timestamp.h"

#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <string>

class Channel;
class EventLoop;
class Socket;

/*
TcpServer => Acceptor=>有一个新用户连接，通过accept函数拿到connfd
打包TcpConnection 设置相应的回调 =》channel=》poller=》channel的回调操作
*/

//网络库底层的缓冲区
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                  const std::string &name,
                  int sockfd,
                  const InetAddress &localAddr,
                  const InetAddress &peerAddr);

    ~TcpConnection();
    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }
    //发送数据
    // void send(const void *message, int len);
    void send(const std::string &buf);
    //关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback &cb) { connectioncallback_ = cb; }

    void setMessageCallback(const MessageCallback &cb) { messagecallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writecompletecallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highwatermark)
    {
        highwatermarkcallback_ = cb;
        highWaterMark_ = highwatermark;
    }

    void setCloseCallback(const CloseCallback &cb) { closecallbak_ = cb; }

    //连接建立
    void connectEstablished();
    //连接销毁
    void connectDestroy();

    void sendInLoop(const void *message, size_t len);
    void shutdownInLoop();

private:
    enum StateE
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };
    void setState(StateE state) { state_ = state; }
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    EventLoop *loop_; //这里绝对不是baseloop，因为tcpconnection都是在subloop里面管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectioncallback_;       //有新连接时的回调
    MessageCallback messagecallback_;             //有读写消息时的回调
    WriteCompleteCallback writecompletecallback_; //消息发送完成以后的回调
    CloseCallback closecallbak_;
    HighWaterMarkCallback highwatermarkcallback_;

    size_t highWaterMark_;
    Buffer inputBuffer_;  //接收数据缓冲区
    Buffer outputBuffer_; //发送数据的缓冲区
};