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

    enum class ChannelOP:char16_t
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
        };
        
        EventLoop();

        EventLoop(std::string name);
        
        ~EventLoop(); 
        
        int32_t run();

        int32_t addTask(std::unique_ptr<net::Channel> channel ,ChannelOP type);
        
        int32_t active(int fd,int32_t event);
        
        int32_t processTaskQ();

        void shutdown();
    private:   
        void taskWakeup_();
        void readLocalMessage_();

        int32_t add_(std::unique_ptr<net::Channel> channel);
        int32_t remove_(int fd);
        int32_t modify_(int fd);
    private:
        Dispatcher* dispatcher_;

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
