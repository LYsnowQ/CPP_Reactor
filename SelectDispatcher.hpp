#pragma once

#include "EventLoop.hpp"
#include "Dispatcher.hpp"
#include <cstdint>
#include <sys/select.h>



namespace reactor::core
{
    class SelectDispatcher:public Dispatcher
    {   
    public:
        SelectDispatcher(EventLoop* evLoop);
        
        int32_t add() override;
        int32_t remove() override;
        int32_t modify() override;
        
        int32_t dispatch(int timeout = 2) override;
   
    private:
        void setFdSet_();
        void clearFdSet_();

    private:
        fd_set readSet_;
        fd_set writeSet_;
        int32_t maxSize_ = 1024;
    };

}// namespace reactor::core
