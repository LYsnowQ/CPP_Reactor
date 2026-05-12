#pragma once
#include "persistence/ThreadLocalSqlConn.hpp"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace reactor::persistence
{
    class SqlExecutor
    {
    public:
        using Job = std::function<void(ThreadLocalSingleConn&)>;
        explicit SqlExecutor(ThreadLocalSingleConn tls);
        ~SqlExecutor();

        void start();

        void stop();

        bool submit(Job job);
    private:
        void run_();

    private:
        ThreadLocalSingleConn tls_;
        std::atomic<bool> running_{false};
        bool stopping_{false};
        std::thread worker_;
        std::mutex mutex_;
        std::condition_variable cv_;
        std::queue<Job> jobQueue_;
    };

}//namespace reactor::persistence
