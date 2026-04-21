#include "Channel.hpp"
#include <cstdint>
#include <unistd.h>

reactor::net::Channel::Channel(int fd,FDEvent events,Callback readCallback,Callback writeCallback,Callback destroyCallback)
    :fd_(fd),
    events_(static_cast<uint32_t>(events)),
    readCallback_(std::move(readCallback)),
    writeCallback_(std::move(writeCallback)),
    destroyCallback_(std::move(destroyCallback))
{}


reactor::net::Channel::~Channel()
{
    if(fd_ >= 0)
    {
        close(fd_);
    }
    destroyFunc();
}


bool reactor::net::Channel::haveReadCallback()
{
    return readCallback_ ? true : false;
}


bool reactor::net::Channel::haveWriteCallback()
{
    return writeCallback_ ? true : false;
}


bool reactor::net::Channel::haveDestroyCallback()
{
    return destroyCallback_ ? true : false;
}


void reactor::net::Channel::readFunc()
{
    if(readCallback_)readCallback_();
}


void reactor::net::Channel::writeFunc()
{
    if(writeCallback_)writeCallback_();
}


void reactor::net::Channel::destroyFunc()
{
    if(destroyCallback_)destroyCallback_();
}



void reactor::net::Channel::writeEventEnable(bool flag)
{
    if(flag)
    {
        events_ |= static_cast<uint32_t>(FDEvent::kWriteEvent);
    }
    else 
    {
        events_ &= ~static_cast<uint32_t>(FDEvent::kWriteEvent);
    }
}


uint32_t reactor::net::Channel::getEvent() const
{
    return events_;
}
   

int reactor::net::Channel::getSocket() const
{
    return fd_;
}
