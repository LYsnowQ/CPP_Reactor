#pragma once

#include "EventLoop.hpp"
#include "Dispatcher.hpp"
#include <cstdint>
#include <vector>
#include <sys/epoll.h>



namespace reactor::core
{
    class EpollDispatcher:public Dispatcher
    {   
    public:
        EpollDispatcher(EventLoop* evLoop);
         ~EpollDispatcher();
        
        StatusCode add() override;
        StatusCode remove() override;
        StatusCode modify() override;
        
        StatusCode dispatch(int timeout = 2) override;
    
    private:
        int32_t epollCtl_(int32_t op);

    private:
        int epfd_;
        int maxNode_;
        std::vector<epoll_event>readyEvents_;
    };

}//namespace reactor::core
