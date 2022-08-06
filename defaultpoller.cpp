#include "poller.h"
#include "EPollPoller.h"

#include <stdio.h>

Poller *Poller::newDefautPoller(EventLoop *loop)
{
    if (getenv("MUDUO_USE_POLL"))
    {
        return nullptr; //生成poll的实例
    }
    else
    {
        return new EPollPoller(loop); //生成epoll的实例
    }
}