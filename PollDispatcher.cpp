#include "PollDispatcher.hpp"
#include "Channel.hpp"


#include <cstdint>
#include <sys/types.h>
#include <system_error>
#include <sys/socket.h>
#include <sys/poll.h>
#include <unistd.h>


reactor::core::PollDispatcher::PollDispatcher(EventLoop* evLoop)
:Dispatcher(evLoop),
maxfd_(0)
{
    fds_ = new struct pollfd[maxNode_];
    for(int32_t i = 0; i < maxNode_; i++)
    {
        fds_[i].fd = -1;
        fds_[i].events = 0;
        fds_[i].revents = 0;  
    }
    name_ = "Poll";
}


reactor::core::PollDispatcher::~PollDispatcher()
{
    if(fds_ != nullptr)
    {
        delete[] fds_;
    }
    
}

int32_t reactor::core::PollDispatcher::add() 
{
    int events = 0;
    if(channel_->getEvent() & static_cast<uint32_t>(net::FDEvent::kReadEvent))
    {
        events |=POLLIN;
    }

    if(channel_->getEvent() & static_cast<uint32_t>(net::FDEvent::kWriteEvent))
    {
        events |=POLLOUT;
    }

    int32_t i = 0;
    for(; i < maxNode_;i++)
    {
        if(fds_[i].fd == -1)
        {
            fds_[i].events = events;
            fds_[i].fd = channel_->getSocket();
            maxfd_ = i > maxfd_ ? i : maxfd_;
            break;
        }
    }
    
    if(i >= maxNode_)
    {
        return -1;
    }

    return 0;
}


int32_t reactor::core::PollDispatcher::remove() 
{
    int32_t i = 0;
    for(; i<maxNode_;i++)
    {
        if(fds_[i].fd == channel_->getSocket())
        {
            fds_[i].events = 0;
            fds_[i].revents = 0;
            fds_[i].fd = -1;
            break;
        }
    }
    if(i >= maxNode_)
    {
        return -1;
    }

    return 0;

}


int32_t reactor::core::PollDispatcher::modify() 
{
    int events = 0;
    if(channel_->getEvent() & static_cast<uint32_t>(net::FDEvent::kReadEvent))
    {
        events |=POLLIN;
    }

    if(channel_->getEvent() & static_cast<uint32_t>(net::FDEvent::kWriteEvent))
    {
        events |=POLLOUT;
    }

    int i = 0;
    for(; i < maxNode_;i++)
    {
        if(fds_[i].fd == channel_->getSocket())
        {
            fds_[i].events = events;
            break;
        }
    }
    
    if(i >= maxNode_)
    {
        return -1;
    }

    return 0;

}
        

int32_t reactor::core::PollDispatcher::dispatch(int timeout) 
{
    int count = poll(fds_, maxNode_, timeout*1000);
    if(count == -1)
    {
        std::system_error(
                errno,
                std::system_category()
                );
    } 
    for(int i = 0; i < maxNode_ ;i++)
    {
        uint32_t ev = 0;
        if(fds_[i].fd == -1)
        { 
            continue;
        }
        
        if(fds_[i].revents & POLLIN)
        {
            ev |= static_cast<uint32_t>(net::FDEvent::kReadEvent);
        }
        if(fds_[i].revents & POLLOUT)
        {
            ev |= static_cast<uint32_t>(net::FDEvent::kWriteEvent);
        }
        if(ev)
        {
            evLoop_->active(fds_[i].fd, ev);
        }
    }

    return 0;
}
    

