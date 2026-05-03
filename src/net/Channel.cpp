#include "net/Channel.hpp"
#include <cstdint>
#include <unistd.h>

namespace reactor::net
{
Channel::Channel(int fd,FDEvent events,Callback readCallback,Callback writeCallback,Callback destroyCallback)
    :fd_(fd),
    events_(static_cast<uint32_t>(events)),
    readCallback_(std::move(readCallback)),
    writeCallback_(std::move(writeCallback)),
    destroyCallback_(std::move(destroyCallback))
{}


Channel::~Channel()
{
    if(fd_ >= 0)
    {
        close(fd_);
    }
    destroyFunc();
}


bool Channel::haveReadCallback()
{
    return readCallback_ ? true : false;
}


bool Channel::haveWriteCallback()
{
    return writeCallback_ ? true : false;
}


bool Channel::haveDestroyCallback()
{
    return destroyCallback_ ? true : false;
}


void Channel::readFunc()
{
    if(readCallback_)readCallback_();
}


void Channel::writeFunc()
{
    if(writeCallback_)writeCallback_();
}


void Channel::destroyFunc()
{
    if(destroyCallback_)destroyCallback_();
}



void Channel::writeEventEnable(bool flag)
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


uint32_t Channel::getEvent() const
{
    return events_;
}
   

int Channel::getSocket() const
{
    return fd_;
}
}
// namespace reactor::net
