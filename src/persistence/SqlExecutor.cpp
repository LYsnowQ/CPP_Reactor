#include "persistence/SqlExecutor.hpp"
#include "persistence/ThreadLocalSqlConn.hpp"
#include <mutex>
#include "spdlog/spdlog.h"


namespace reactor::persistence
{
    // ====================================================================
    // 构造
    // ====================================================================
    //
    // ThreadLocalSingleConn 通过 move 转移所有权。
    // start() 之前不创建工作线程，任务队列为空。
    SqlExecutor::SqlExecutor(ThreadLocalSingleConn tls)
        :tls_(std::move(tls)){}

    SqlExecutor::~SqlExecutor()
    {
        stop();
    }

    // ====================================================================
    // 启动工作线程
    // ====================================================================
    //
    // running_ 标志用 atomic exchange 做幂等保护。
    // 先设 stopping_=false，再创建线程，确保新线程能看到正确的停止状态。
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

    // ====================================================================
    // 停止工作线程
    // ====================================================================
    //
    // 先设 running_=false → 再设 stopping_=true → notify 条件变量 → join。
    // 注意顺序：notify 时 stopping_ 必须已为 true，否则 run_ 可能错过停止信号。
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

    // ====================================================================
    // 任务提交
    // ====================================================================
    //
    // 加锁入队后 notify_one 唤醒工作线程。
    // 若 stopping_=true 返回 false，调用方可根据返回值决定是否重试或丢弃。
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

    // ====================================================================
    // 工作线程主循环
    // ====================================================================
    //
    // 1. 加锁等待条件变量：
    //    - stopping_=true 且队列为空 → 退出循环
    //    - 队列非空 → 取出一个 job
    // 2. 解锁后执行 job(tls_)。
    // 3. 使用 try-catch 保护 job 执行，异常仅记录 spdlog::warn 不传播。
    // 4. 退出循环后调用 tls_.cleanupCurrentThread() 释放线程局部连接。
    void SqlExecutor::run_()
    {
        while(true)
        {
            Job job;
            {
                std::unique_lock<std::mutex> lk(mutex_);
                cv_.wait(lk,[this]{return stopping_ || !jobQueue_.empty();});
                // 停止 + 队列空 → 正常退出。
                // 若停止但队列仍有剩余任务，处理完再退出。
                if(stopping_ && jobQueue_.empty())
                {
                    break;
                }
                job = std::move(jobQueue_.front());
                jobQueue_.pop();
            }
            // 需要在锁外执行 job，避免长时间持有锁。
            try
            {
                job(tls_);
            }
            // job 内部异常不传播，单个 job 失败不影响后续任务执行。
            catch(const std::exception& err)
            {
                spdlog::warn("SqlExecutor job failed: {}",err.what());
            }
        }
        tls_.cleanupCurrentThread();
    }

}//namespace reactor::persistence
