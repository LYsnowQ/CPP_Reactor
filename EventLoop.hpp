#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <sys/socket.h>
#include <queue>
#include <thread>
#include <map>
#include <mutex>
#include <atomic>
#include "Channel.hpp"
#include "Dispatcher.hpp"

namespace reactor::core
{

    enum class ChannelOP:uint8_t
    {
        ADD,
        DELETE,
        MODIFY
    };

    class Dispatcher;

    class EventLoop
    {
    public:
        
        struct ChannelElement
        {
            ChannelOP type;
            std::unique_ptr<net::Channel> channel;
            int fd;
        };
        
        EventLoop();

        EventLoop(std::string name, DispatcherType type = DispatcherType::kEpoll);
        
        ~EventLoop(); 
        
        StatusCode run();

        StatusCode addTask(std::unique_ptr<net::Channel> channel ,ChannelOP type);
        
        StatusCode addTask(int fd,ChannelOP type);

        StatusCode destroyTask(int fd);        

        StatusCode active(int fd,uint32_t event);
        
        StatusCode processTaskQ();

        void shutdown();
    private:   
        void taskWakeup_();
        void readLocalMessage_();

        StatusCode add_(std::unique_ptr<net::Channel> channel);
        StatusCode remove_(int fd);
        StatusCode modify_(int fd);
    private:
        std::unique_ptr<Dispatcher> dispatcher_;

        // 任务队列
        std::queue<ChannelElement> taskQ_;
       
        std::string threadName_;    
        std::thread::id threadID_;
        
        //映射与互斥锁:
        std::map<int,std::unique_ptr<net::Channel>>channelMap_;
        std::mutex mutex_;
        std::atomic<bool> isQuit_{true};
      
        int socketPair_[2]; // 存储由 socketpair 初始化的本地通信 fd
    };


} //namespace reactor::core
