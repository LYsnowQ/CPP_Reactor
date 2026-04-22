#pragma once

#include <functional>
#include <cstdint>
#include <type_traits>


namespace reactor::net 
{
    enum class FDEvent:uint32_t
    {
        kTimeout = 0x01,
        kReadEvent = 0x02,
        kWriteEvent = 0x04,
        kErrorEvent = 0x08
    };

    class Channel
    {
    public:
        using Callback = std::function<void()>;

        // 初始化一个 Channel
        Channel(int fd,
                FDEvent events,
                Callback readCallback ,
                Callback writeCallback,
                Callback destroyCallback
                );
        ~Channel();

        void writeEventEnable(bool flag);

        inline bool isWriteEventEnable() const;
        
        uint32_t getEvent() const;
        int getSocket() const;

        void readFunc();
        void writeFunc();
        void destroyFunc();

        bool haveReadCallback();
        bool haveWriteCallback();
        bool haveDestroyCallback();

    private:
        int fd_;
        uint32_t events_;
        Callback readCallback_;
        Callback writeCallback_;
        Callback destroyCallback_;
    };

    bool Channel::isWriteEventEnable() const
    {
        return events_ & static_cast<uint32_t>(FDEvent::kWriteEvent); 
    }

        
}//namespace reactor::net
