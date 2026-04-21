#pragma once


#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include <cstdint>
#include <thread>
#include <cstdint>
#include <atomic>
#include <queue>
#include <future>
#include <functional>
#include <condition_variable>
#include <latch>//cpp20特性计数器，在非0时会阻塞

#include "EventLoop.hpp"


namespace reactor::net{
    class IOThreadPool
    {
    public:
        using Task = std::function<void()>;

        IOThreadPool(uint32_t maxThreads = 2);
        ~IOThreadPool();

        void start();
        void stop();

        core::EventLoop* getNextLoop();
        //core::EventLoop* getLoop(uint32_t index);长连接暂时不实现

        //运行移动，但禁止拷贝
        IOThreadPool(const IOThreadPool&) = delete;
        IOThreadPool& operator= (const IOThreadPool&) = delete;
        IOThreadPool(const IOThreadPool&&) = delete;
    private:
        void worker_(size_t index);

    private:
        std::vector<std::thread>threads_;
        std::vector<std::unique_ptr<core::EventLoop>>loops_;
        std::queue<Task>taskQ_;
        
        std::condition_variable cv_;
        std::mutex mutex_;

        std::atomic<bool> isStop_{false};
        std::latch latch_;
        uint32_t loopIndex_;
        const uint32_t maxThreads_;
    };
    
}//namespace reactor::net
