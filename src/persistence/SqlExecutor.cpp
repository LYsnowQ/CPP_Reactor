#include "persistence/SqlExecutor.hpp"
#include "persistence/ThreadLocalSqlConn.hpp"
#include <mutex>
#include "spdlog/spdlog.h"


namespace reactor::persistence
{
    SqlExecutor::SqlExecutor(ThreadLocalSingleConn tls)
        :tls_(std::move(tls)){}

    SqlExecutor::~SqlExecutor()
    {
        stop();
    }

    void SqlExecutor::start()
    {
        if(running_.exchange(true))
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lk(mutex_);
            stopping_ = false;
        }

        worker_ = std::thread ([this](){ run_();});
    }

    void SqlExecutor::stop()
    {
        if(!running_.exchange(false))
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lk(mutex_);
            stopping_ = true;
        }
        cv_.notify_one();

        if(worker_.joinable())
        {
            worker_.join();
        }
    }

    bool SqlExecutor::submit(Job job)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if(stopping_)
        {
            return false;
        }
        jobQueue_.push(std::move(job));
        cv_.notify_one();
        return true;
    }

    void SqlExecutor::run_()
    {
        while(true)
        {
            Job job;
            {
                std::unique_lock<std::mutex> lk(mutex_);
                cv_.wait(lk,[this]{return stopping_ || !jobQueue_.empty();});
                if(stopping_ && jobQueue_.empty())
                {
                    break;
                }
                job = std::move(jobQueue_.front());
                jobQueue_.pop();
            }
            try
            {
                job(tls_);
            }
            catch(const std::exception& err)
            {
                spdlog::warn("SqlExecutor job failed: {}",err.what());
            }
        }
        tls_.cleanupCurrentThread();
    }

}//namespace reactor::persistence
