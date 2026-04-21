#pragma once
#include "Channel.hpp"
#include "EventLoop.hpp"

#include <cstdint>
#include <memory>
#include <string>


namespace reactor::core
{
    class EventLoop;

    class Dispatcher
    {   
    public:
        Dispatcher(EventLoop* evLoop);
        virtual ~Dispatcher() = default;
        
        virtual int32_t add() = 0;
        virtual int32_t remove() = 0;
        virtual int32_t modify() = 0;
        
        virtual int32_t dispatch(int timeout = 2) = 0;

        inline void setChannel(net::Channel* channel);
    protected:
        std::string name_ = std::string();
        EventLoop* evLoop_;//观察者，生命周期由tcpserver掌管不用智能指针，其拥有dispatcher
        net::Channel* channel_;
    };

    //内部使用时不对其进行创建和销毁，则在此处我们使用原始指针，其销毁交给channelMap_
    void Dispatcher::setChannel(net::Channel* channel)
    {
        channel_ = channel;
    }
}//namespace reactor::core
