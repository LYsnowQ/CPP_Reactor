#include "net/IOThreadPool.hpp"

#include <cstdint>
#include <latch>
#include <memory>
#include <system_error>

namespace reactor::net
{
    IOThreadPool::IOThreadPool(uint32_t maxThreads, core::DispatcherType dispatcherType)
        : loops_(maxThreads), latch_(maxThreads), loopIndex_(0), maxThreads_(maxThreads),
          dispatcherType_(dispatcherType)
    {
    }

    IOThreadPool::~IOThreadPool()
    {
        stop();
    }

    // ====================================================================
    // 启动所有 Worker 线程
    // ====================================================================
    //
    // 用 latch 同步所有 worker 的就绪状态：
    //   1. 先创建所有线程（每个线程执行 worker_）
    //   2. 调用 latch.wait() 阻塞，直到所有 worker 完成 EventLoop 构造
    // 这确保 getNextLoop 返回的 EventLoop 对象一定可用。
    //
    // 注意：worker_ 中 EventLoop::run() 会阻塞，所以 latch.count_down()
    // 必须在 run() 之前调用。
    void IOThreadPool::start()
    {
        if (!threads_.empty())
        {
            return;
        }

        isStop_.store(false);
        for (size_t i = 0; i < loops_.size(); i++)
        {
            threads_.emplace_back(&IOThreadPool::worker_, this, i);
        }
        latch_.wait();
    }

    // ====================================================================
    // 停止所有 Worker 线程
    // ====================================================================
    //
    // 先通过 EventLoop::shutdown() 设置退出标志并唤醒，
    // 再逐个 join 线程。shutdown 和 join 的顺序不能颠倒——
    // 若先 join，线程可能因 epoll_wait 阻塞而无法退出。
    void IOThreadPool::stop()
    {
        if (threads_.empty())
        {
            return;
        }

        isStop_.store(true);
        for (auto &loop : loops_)
        {
            if (loop)
            {
                loop->shutdown();
            }
        }
        for (auto &thread : threads_)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        threads_.clear();
    }

    // ====================================================================
    // Round-Robin 负载均衡
    // ====================================================================
    //
    // 简单轮询分配，不考虑当前各 EventLoop 的实际负载。
    // loopIndex_ 不在多线程下保护——getNextLoop 仅在 accept 线程调用，
    // accept 是单线程的，不存在竞争。
    core::EventLoop *IOThreadPool::getNextLoop()
    {
        core::EventLoop *evLoop = loops_[loopIndex_].get();
        loopIndex_++;
        loopIndex_ %= maxThreads_;
        return evLoop;
    }

    void IOThreadPool::worker_(size_t index)
    {
        loops_[index] = std::make_unique<core::EventLoop>(
            std::string("worker" + std::to_string(index)), dispatcherType_);
        latch_.count_down();
        loops_[index]->run();
    }

} // namespace reactor::net
// namespace reactor::net
