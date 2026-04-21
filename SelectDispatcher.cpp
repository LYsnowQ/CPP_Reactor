#include "SelectDispatcher.hpp"
#include "Channel.hpp"

#include <cstdint>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <system_error>



reactor::core::SelectDispatcher::SelectDispatcher(EventLoop* evLoop)
:Dispatcher(evLoop)
{
    FD_ZERO(&readSet_);
    FD_ZERO(&writeSet_);
    name_ = "Select";
}


void reactor::core::SelectDispatcher::setFdSet_()
{
    int fd = channel_->getSocket();
    if(channel_->getEvent() & static_cast<uint32_t>(net::FDEvent::kReadEvent))
    {
        FD_SET(fd,&readSet_);
    }

    if(channel_->getEvent() & static_cast<uint32_t>(net::FDEvent::kWriteEvent))
    {
        FD_SET(fd,&writeSet_);
    }
}

void reactor::core::SelectDispatcher::clearFdSet_()
{
    int fd = channel_->getSocket();
    if(channel_->getEvent() & static_cast<uint32_t>(net::FDEvent::kReadEvent))
    {
        FD_CLR(fd,&readSet_);
    }

    if(channel_->getEvent() & static_cast<uint32_t>(net::FDEvent::kWriteEvent))
    {
        FD_CLR(fd,&writeSet_);
    }
    
}


int32_t reactor::core::SelectDispatcher::add()
{
    if(channel_->getSocket() >= maxSize_)
    {
        return -1;
    }
    setFdSet_();

    return 0;
}


int32_t reactor::core::SelectDispatcher::remove()
{
    clearFdSet_();
    return 0;
}


int32_t reactor::core::SelectDispatcher::modify()
{
    clearFdSet_();
    setFdSet_();
    return 0;
}


int32_t reactor::core::SelectDispatcher::dispatch(int timeout)
{
    struct timeval val;
    val.tv_sec = timeout;
    val.tv_usec = 0;
    fd_set rdtmp = readSet_;
    fd_set wrtmp = writeSet_;
    int32_t count = select(maxSize_, &rdtmp, &wrtmp,nullptr,&val);
    if(count == -1)
    {
        throw std::system_error(
                errno,
                std::system_category()
                );
        exit(0);
    } 
    for(int i = 0; i < maxSize_ ;i++)
    {
        uint32_t ev = 0;
        if(FD_ISSET(i,&rdtmp))
        { 
            ev |= static_cast<uint32_t>(net::FDEvent::kReadEvent);
        }
        
        if(FD_ISSET(i, &wrtmp))
        {
            ev |= static_cast<uint32_t>(net::FDEvent::kWriteEvent);
        }
        if(ev)
        {
            evLoop_->active(i ,ev);
        }
    }

    return 0;
}
