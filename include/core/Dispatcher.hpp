#pragma once
#include "Channel.hpp"
#include "CoreStatus.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>


namespace reactor::core
{
    class EventLoop;

    enum class DispatcherType : uint8_t
    {
        kEpoll,
        kPoll,
        kSelect
    };

    class Dispatcher
    {   
    public:
        Dispatcher(EventLoop* evLoop);
        virtual ~Dispatcher() = default;
        
        virtual StatusCode add() = 0;
        virtual StatusCode remove() = 0;
        virtual StatusCode modify() = 0;
        
        virtual StatusCode dispatch(int timeout = 2) = 0;

        inline void setChannel(net::Channel* channel);
    protected:
        std::string name_ = std::string();
        EventLoop* evLoop_;//观察者，生命周期由tcpserver掌管不用智能指针，其拥有dispatcher
        net::Channel* channel_;
    };

    std::unique_ptr<Dispatcher> createDispatcher(EventLoop* evLoop, DispatcherType type);
    DispatcherType dispatcherTypeFromString(std::string_view name, DispatcherType fallback = DispatcherType::kEpoll);
    const char* dispatcherTypeToString(DispatcherType type);

    //内部使用时不对其进行创建和销毁，则在此处我们使用原始指针，其销毁交给channelMap_
    void Dispatcher::setChannel(net::Channel* channel)
    {
        channel_ = channel;
    }
}//namespace reactor::core
