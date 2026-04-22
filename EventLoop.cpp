#include "EventLoop.hpp"
#include "Channel.hpp"
#include "Dispatcher.hpp"

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
#include <string.h>
#include <cerrno>
#include <fcntl.h>
#include <stdexcept>


reactor::core::EventLoop::EventLoop():EventLoop("MainThread")
{}


reactor::core::EventLoop::EventLoop(std::string name, DispatcherType type)
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
                std::system_category(),
                "socketpair failed in EventLoop"
                );
    }

    // 线程唤醒使用非阻塞，防止跨线程阻塞
    int flags0 = fcntl(socketPair_[0], F_GETFL, 0);
    int flags1 = fcntl(socketPair_[1], F_GETFL, 0);
    if(flags0 != -1)
    {
        fcntl(socketPair_[0], F_SETFL, flags0 | O_NONBLOCK);
    }
    if(flags1 != -1)
    {
        fcntl(socketPair_[1], F_SETFL, flags1 | O_NONBLOCK);
    }
    dispatcher_ = createDispatcher(this, type);
    if(!dispatcher_)
    {
        throw std::runtime_error("create dispatcher failed");
    }

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
}


// 本地写数据
void reactor::core::EventLoop::taskWakeup_()
{
    char msg = 'w';
    ssize_t n = write(socketPair_[1], &msg, sizeof(msg));
    (void)n;
}

// 本地读数据
void reactor::core::EventLoop::readLocalMessage_()
{
    char buf[256];
    while(true)
    {//避免信号堆积
        ssize_t n = read(socketPair_[0], buf, sizeof(buf));
        if(n <= 0)
        {
            break;
        }
    }
}


reactor::core::StatusCode reactor::core::EventLoop::run()
{
    isQuit_.store(false);//延迟启动非初始化时启动
    if(std::this_thread::get_id() != threadID_)
    {
        return StatusCode::kError;
    }

    while(!isQuit_)
    {
        if(dispatcher_->dispatch() == StatusCode::kError)
        {
            return StatusCode::kError;
        }
        processTaskQ();
    }

    return StatusCode::kOk;

}


reactor::core::StatusCode reactor::core::EventLoop::active(int fd,uint32_t event)
{
    if(fd < 0)
    {
        return StatusCode::kInvalid;
    }

    auto it= channelMap_.find(fd);
    if(it == channelMap_.end() || !it->second)
    {
        return StatusCode::kNotFound;
    }
    auto* channel = it->second.get();

    if(event & static_cast<uint32_t>(net::FDEvent::kErrorEvent))
    {
        return remove_(fd);
    }

    if(event & static_cast<uint32_t>(net::FDEvent::kReadEvent) && channel->haveReadCallback())
    {
        channel->readFunc();
    }

    if(event & static_cast<uint32_t>(net::FDEvent::kWriteEvent) && channel->haveWriteCallback())
    {
        channel->writeFunc();
    }

    return StatusCode::kOk;
}


reactor::core::StatusCode reactor::core::EventLoop::addTask(std::unique_ptr<net::Channel> channel,ChannelOP type)
{
    {    
        std::lock_guard<std::mutex> lk(mutex_);
        taskQ_.push(ChannelElement{type,std::move(channel),-1});
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
    return StatusCode::kOk;
}


reactor::core::StatusCode reactor::core::EventLoop::addTask(int fd,ChannelOP type)
{
    if(fd < 0)
    {
        return StatusCode::kInvalid;
    }
    if(type == ChannelOP::ADD)
    {
        return StatusCode::kInvalid;
    }

    {    
        std::lock_guard<std::mutex> lk(mutex_);
        taskQ_.push(ChannelElement{type,nullptr,fd});
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
    return StatusCode::kOk;
}


reactor::core::StatusCode reactor::core::EventLoop::destroyTask(int fd)
{
    return addTask(fd, ChannelOP::DELETE);
}

reactor::core::StatusCode reactor::core::EventLoop::processTaskQ()
{
    std::queue<ChannelElement> localQ;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        std::swap(localQ, taskQ_);
    }

    while(!localQ.empty())
    {
        auto element = std::move(localQ.front());
        localQ.pop();
        if(element.type == ChannelOP::ADD)
        {
            add_(std::move(element.channel)); 
        }
        else if(element.type == ChannelOP::DELETE)
        {
            int fd = element.fd;
            if(fd < 0 && element.channel)
            {
                fd = element.channel->getSocket();
            }
            remove_(fd); 
        }
        else if(element.type == ChannelOP::MODIFY)
        {
            int fd = element.fd;
            if(fd < 0 && element.channel)
            {
                fd = element.channel->getSocket();
            }
            modify_(fd);
        }
    }
    return StatusCode::kOk;
}


void reactor::core::EventLoop::shutdown()
{
    isQuit_.store(true);
    taskWakeup_();
}



reactor::core::StatusCode reactor::core::EventLoop::add_(std::unique_ptr<net::Channel> channel)
{
    int fd = channel -> getSocket();
    
    if(channelMap_.find(fd) == channelMap_.end())
    {
        channelMap_.insert(std::make_pair(fd,std::move(channel)));
        dispatcher_->setChannel(channelMap_[fd].get());
        if(dispatcher_->add() != StatusCode::kOk)
        {
            channelMap_.erase(fd);
            return StatusCode::kError;
        }
        return StatusCode::kOk;
    }

    return StatusCode::kError;
}


reactor::core::StatusCode reactor::core::EventLoop::remove_(int fd)
{
    if(channelMap_.find(fd) == channelMap_.end())
    {
        return StatusCode::kNotFound;
    }
    dispatcher_->setChannel(channelMap_[fd].get());
    StatusCode ret = dispatcher_->remove();
    if(ret == StatusCode::kOk)
    {
        channelMap_.erase(fd);
    }
    return ret;
}


reactor::core::StatusCode reactor::core::EventLoop::modify_(int fd)
{
    if(channelMap_.find(fd)==channelMap_.end())
    {
        return StatusCode::kNotFound;
    }
    dispatcher_->setChannel(channelMap_[fd].get());
    StatusCode ret = dispatcher_->modify();
    return ret;
}
