#include "EventLoop.hpp"
#include "Channel.hpp"
#include "EpollDispatcher.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>


reactor::core::EventLoop::EventLoop():EventLoop("MainThread")
{}


reactor::core::EventLoop::EventLoop(std::string name)
:
threadName_(name),
threadID_(std::this_thread::get_id())
{
    socketPair_[0] = -1;
    socketPair_[1] = -1;
    int32_t ret = socketpair(AF_UNIX,SOCK_STREAM,0,socketPair_);
    if(ret == -1)
    {
        throw std::system_error(
                errno,
                std::system_category()
                );
    }
    dispatcher_ = new EpollDispatcher(this);

    auto obj = std::bind(&EventLoop::readLocalMessage_,this);
    std::unique_ptr<net::Channel>channel =
        std::make_unique<net::Channel>
        (socketPair_[0],
         net::FDEvent::kReadEvent,
         obj,
         nullptr,
         nullptr
        );
    addTask(std::move(channel), ChannelOP::ADD);
}


reactor::core::EventLoop::~EventLoop()
{
    if(socketPair_[1]>=0)
    {
        close(socketPair_[1]);
    }
    if(dispatcher_)
    {
        delete dispatcher_;
    }
}


// 本地写数据
void reactor::core::EventLoop::taskWakeup_()
{
    const char* msg = "wakeUp!";
    write(socketPair_[1],msg,strlen(msg));
}

// 本地读数据
void reactor::core::EventLoop::readLocalMessage_()
{
    char buf[256];
    read(socketPair_[0],buf,sizeof(buf));
}


int32_t reactor::core::EventLoop::run()
{
    isQuit_.store(false);//延迟启动非初始化时启动
    if(std::this_thread::get_id() != threadID_)
    {
        return -1;
    }

    while(!isQuit_)
    {
        dispatcher_->dispatch();
        processTaskQ();
    }

    return 0;

}


int32_t reactor::core::EventLoop::active(int fd,int32_t event)
{
    if(fd < 0)
    {
        return -1;
    }

    auto it= channelMap_.find(fd);
    if(it == channelMap_.end() || !it->second)
    {
        throw std::system_error(
                errno,
                std::system_category()
                );
    }
    auto* channel = it->second.get();

    if(event & static_cast<uint32_t>(net::FDEvent::kReadEvent) && channel->haveReadCallback())
    {
        channel->readFunc();
    }

    if(event & static_cast<uint32_t>(net::FDEvent::kWriteEvent) && channel->haveWriteCallback())
    {
        channel->writeFunc();
    }

    return 0;
}


int32_t reactor::core::EventLoop::addTask(std::unique_ptr<net::Channel> channel,ChannelOP type)
{
    {    
        std::lock_guard<std::mutex> lk(mutex_);
        taskQ_.emplace(type,std::move(channel));
    }
    // 细节处理:
    // 对于往链表中新增节点，可能由当前线程处理，也可能别的线程处理。
    //   1) 修改 fd 事件的操作，可能由当前线程发起，也由当前线程处理。
    //   2) 新增 fd 的操作由主线程发起。

    if(threadID_ == std::this_thread::get_id())
    {
        // 当前线程直接处理
        processTaskQ();
    }
    else 
    {
        // 跨线程需要唤醒
        taskWakeup_(); 
    }
    return 0;
}


int32_t reactor::core::EventLoop::destroyTask(int fd)
{
    if(channelMap_.find(fd) == channelMap_.end())
    {
        throw std::system_error(
                    errno,
                    std::system_category(),
                    "fd dosen't exist"
                );
        return -1;
    }
    
    channelMap_.erase(fd);    
    return 0;
}

int32_t reactor::core::EventLoop::processTaskQ()
{
    std::unique_lock<std::mutex> lk(mutex_);

    while(!taskQ_.empty())
    {
        auto element = std::move(taskQ_.front());
        taskQ_.pop();
        lk.unlock();
        if(element.type == ChannelOP::ADD)
        {
            add_(std::move(element.channel)); 
        }
        else if(element.type == ChannelOP::DELETE)
        {
            int fd = element.channel->getSocket();
            remove_(fd); 
        }
        else if(element.type == ChannelOP::MODIFY)
        {
            int fd = element.channel->getSocket();
            modify_(fd);
        }
        lk.lock();
    }
    return 0;
}


void reactor::core::EventLoop::shutdown()
{
    isQuit_.store(true);
    taskWakeup_();
}



int32_t reactor::core::EventLoop::add_(std::unique_ptr<net::Channel> channel)
{
    int fd = channel -> getSocket();
    
    if(channelMap_.find(fd) == channelMap_.end())
    {
        channelMap_.insert(std::make_pair(fd,std::move(channel)));
        dispatcher_->setChannel(channelMap_[fd].get());
        dispatcher_->add();
        return 0;
    }

    return -1;
}


int32_t reactor::core::EventLoop::remove_(int fd)
{
    if(channelMap_.find(fd) == channelMap_.end())
    {
        return -1;
    }
    dispatcher_->setChannel(channelMap_[fd].get());
    int32_t ret = dispatcher_->remove();
    return ret;
}


int32_t reactor::core::EventLoop::modify_(int fd)
{
    if(channelMap_.find(fd)==channelMap_.end())
    {
        return -1;
    }
    dispatcher_->setChannel(channelMap_[fd].get());
    int32_t ret = dispatcher_->modify();
    return ret;
}



