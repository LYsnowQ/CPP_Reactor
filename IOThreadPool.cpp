#include "IOThreadPool.hpp"


#include <cstdint>
#include <latch>
#include <memory>
#include <system_error>



reactor::net::IOThreadPool::IOThreadPool(uint32_t maxThreads, core::DispatcherType dispatcherType)
:loops_(maxThreads),
latch_(maxThreads),
loopIndex_(0),
maxThreads_(maxThreads),
dispatcherType_(dispatcherType)
{}


reactor::net::IOThreadPool::~IOThreadPool()
{
    stop();
}


void reactor::net::IOThreadPool::start()
{
    if(!threads_.empty())
    {
        return;
    }

    isStop_.store(false);
    for(size_t i =0; i<loops_.size();i++)
    {
        threads_.emplace_back(&IOThreadPool::worker_,this,i);
    } 
    latch_.wait();
}


void reactor::net::IOThreadPool::stop()
{
    if(threads_.empty())
    {
        return;
    }

    isStop_.store(true);
    for(auto& loop:loops_)
    {
        if(loop)
        {
            loop->shutdown();
        }
    }
    for(auto& thread:threads_ )
    {
        if(thread.joinable())
        {
            thread.join();
        }
    }
    threads_.clear();
}


reactor::core::EventLoop* reactor::net::IOThreadPool::getNextLoop()
{
    core::EventLoop* evLoop = loops_[loopIndex_].get();
    loopIndex_++;
    loopIndex_ %= maxThreads_; 
    return evLoop;
}

//reactor::core::EventLoop* reactor::net::IOThreadPool::getLoop(uint32_t index)
//{}
//暂时不实现长连接获取 

void reactor::net::IOThreadPool::worker_(size_t index)
{
    loops_[index] = std::make_unique<core::EventLoop>(std::string("worker"+std::to_string(index)), dispatcherType_);
    latch_.count_down();
    loops_[index]->run();
}

 
