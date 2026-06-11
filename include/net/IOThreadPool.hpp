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
#include <latch>

#include "core/EventLoop.hpp"

namespace reactor::net
{

/// @brief IO 线程池，管理一组 Worker EventLoop
///
/// 每个 Worker 线程运行一个独立的 EventLoop，采用 round-robin 策略
/// 为接入的连接分配 EventLoop，实现 IO 负载在多个线程间均衡。
///
/// @param maxThreads       Worker 线程数量（默认 2）
/// @param dispatcherType   IO 复用后端类型（默认 epoll）
/// @thread start/stop 由主线程调用，getNextLoop 可被 accept 线程调用
class IOThreadPool
{
  public:
    using Task = std::function<void()>;

    /// @brief 构造 IO 线程池（仅记录参数，不启动线程）
    IOThreadPool(uint32_t maxThreads = 2,
                 core::DispatcherType dispatcherType = core::DispatcherType::kEpoll);

    /// @brief 析构时自动调用 stop()
    ~IOThreadPool();

    /// @brief 启动所有 Worker 线程并等待它们就绪
    ///
    /// 每个 worker 线程中：构造 EventLoop → latch.count_down() → EventLoop::run()。
    /// 所有 worker 就绪后 start() 返回。
    /// @warning 不可重复调用，第二次调用直接返回
    void start();

    /// @brief 停止所有 Worker 线程
    ///
    /// 依次对每个 EventLoop 调用 shutdown() 唤醒退出，然后 join 所有线程。
    /// @warning 不可重复调用，第二次调用直接返回
    void stop();

    /// @brief 按 round-robin 策略获取下一个 Worker EventLoop
    /// @return EventLoop* 非拥有指针（生命周期由 IOThreadPool 管理）
    core::EventLoop *getNextLoop();

    IOThreadPool(const IOThreadPool &) = delete;
    IOThreadPool &operator=(const IOThreadPool &) = delete;
    IOThreadPool(const IOThreadPool &&) = delete;

  private:
    void worker_(size_t index);

  private:
    std::vector<std::thread> threads_;
    std::vector<std::unique_ptr<core::EventLoop>> loops_;
    std::queue<Task> taskQ_;

    std::condition_variable cv_;
    std::mutex mutex_;

    std::atomic<bool> isStop_{false};
    std::latch latch_;
    uint32_t loopIndex_;
    const uint32_t maxThreads_;
    const core::DispatcherType dispatcherType_;
};

} // namespace reactor::net
