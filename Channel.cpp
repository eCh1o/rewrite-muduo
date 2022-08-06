#include "Channel.h"
#include "EventLoop.h"
#include "logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *Loop, int fd)
    : loop_(Loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      tied_(false)
{
}

Channel::~Channel()
{
}

// channel的tie方法什么时候调用过
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

//当改变channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl
void Channel::update()
{
    //通过channel所属的eventloop调用poller的相应方法，注册fd和events事件
    loop_->updateChannel(this);
}

//在channel所属的eventloop中，把当前的channel删除掉
void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receivetime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receivetime);
        }
    }
    else
    {
        handleEventWithGuard(receivetime);
    }
}

//根据poller统治的channel发生的具体事件，有channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receivetime)
{
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    if (revents_ & EPOLLHUP && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
            closeCallback_();
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
            errorCallback_();
    }

    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
            readCallback_(receivetime);
    }

    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
            writeCallback_();
    }
}