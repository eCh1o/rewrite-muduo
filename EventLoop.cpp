#include "EventLoop.h"
#include "logger.h"
#include "poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

//防止一个线程创建多个Eventloop  thread_local
__thread EventLoop *t_loopInThisThread = nullptr;

//定义默认的Poller IO复用接口的超时事件
const int kPollTimeMs = 10000;

//创建wakeupfd，用来notify唤醒subreactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop() : looping_(false),
                         quit_(false),
                         callingPendingFunctors_(false),
                         threadId_(CurrentThread::tid()),
                         poller_(Poller::newDefautPoller(this)),
                         wakeupFd_(createEventfd()),
                         wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another Eventloop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    //设置wakeupfd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    //每一个eventloop都将监听wakeupchannel的EPOLLIN读事件了
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;
    LOG_INFO("Eventloop %p start looping\n", this);
    while (!quit_)
    {
        activeChannels_.clear();
        //监听两类fd 一种是clientfd，一种是wakeupfd
        //mainloop收到新用户的连接会给wakeupfd发送一个数据，解除poll的阻塞
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // poller监听哪些channel发生了事件，然后上报给eventloop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前eventloop事件循环需要处理的回调操作
        /*
        IO线程 mainloop accept fd打包成channel 分发给=>subloop
        mainloop事先注册一个回调cb需要subloop来执行， wakeup subloop执行下面的方法执行之前mainloop注册的操作
        */
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

/*
                           mainloop


        subloop1            subloop2            subloop3
*/
//退出事件循环，loop在自己的线程中调用quit
void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread()) //如果实在其他线程中调用的quit
    {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) //在当前的loop线程中，执行cb
    {
        cb();
    }
    else //在非loop线程中执行cb,就需要唤醒loop所在线程执行cb
    {
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(Functor cb)
{

    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctor_.emplace_back(cb);
    }

    //唤醒相应的需要执行上面回调操作的loop的线程了
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); //唤醒loop所在线程
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %d bytes instead of 8", (int)n);
    }
}

//用来唤醒loop所在的线程向wakeupfd写一个数据
//wakeupChannel就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("Eventloop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors()  //执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctor_);
    }

    for(const Functor &functor : functors)
    {
        functor();  //执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_ = false;
}