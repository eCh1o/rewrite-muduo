#pragma once

#include "poller.h"
#include "timestamp.h"

#include <vector>
#include <sys/epoll.h>

/*
    epoll_create
    epoll_ctl       add/mod/del
    epoll_wait
*/

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    //重写基类poller的抽象方法
    Timestamp poll(int timeout, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16;

    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    //更新channel通道
    void update(int operation, Channel *Channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};