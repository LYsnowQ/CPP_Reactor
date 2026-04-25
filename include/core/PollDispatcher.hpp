#pragma once

#include "Dispatcher.hpp"
#include <cstdint>
#include <sys/poll.h>



namespace reactor::core
{
    class PollDispatcher:public Dispatcher
    {   
    public:
        PollDispatcher(EventLoop* evLoop);
         ~PollDispatcher();
        
        StatusCode add() override;
        StatusCode remove() override;
        StatusCode modify() override;
        
        StatusCode dispatch(int timeout = 2) override;
    
    private:
        int32_t maxfd_;
        struct pollfd* fds_;
        int32_t maxNode_ = 1024;
    };

}//namespace reactor::core
