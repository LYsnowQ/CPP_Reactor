#pragma once

#include <cstdint>
#include <memory>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <vector>

#include "net/TcpConnection.hpp"
#include "core/EventLoop.hpp"
#include "net/IOThreadPool.hpp"
#include "observability/Metrics.hpp"

namespace reactor::net
{

/// @brief TCP 服务端，管理监听 socket + 连接接入 + 连接容器
///
/// 职责：
///   1. 创建监听 socket（socket → setsockopt → bind → listen）
///   2. 启动 IOThreadPool
///   3. 循环 accept 新连接 → 分配 EventLoop → 创建 TcpConnection
///   4. 维护连接容器（conns_），定期清理已关闭/空闲超时的连接
///   5. 每 5 秒输出一次指标日志（QPS、延迟、吞吐量）
///
/// 线程模型：
///   - acceptConnection 在主线程中阻塞运行
///   - Worker 线程各自运行 EventLoop::run() 处理 IO 事件
///   - 连接关闭通过 pendingCloseFds_ 队列同步到主线程清理
///
/// @throw std::system_error socket/bind 失败时抛出
/// @thread run/stop 由主线程调用
class TcpServer
{
  public:
    using RequestHandler = TcpConnection::RequestHandler;

    /// @brief 构造 TCP 服务器
    /// @param port                监听端口
    /// @param maxThread           Worker 线程数
    /// @param dispatcherType      IO 复用后端类型（默认 epoll）
    /// @param keepAliveEnabled    是否启用 Keep-Alive
    /// @param keepAliveMaxRequests Keep-Alive 最大请求数（默认 100，为 0 时调整为 1）
    /// @param keepAliveIdleTimeoutMs 空闲超时毫秒数（默认 10000，为 0 时调整为 1）
    ///
    /// 构造函数中：创建 socket → SO_REUSEADDR → bind → 非阻塞 → 创建 IOThreadPool
    TcpServer(uint16_t port, uint32_t maxThread,
              core::DispatcherType dispatcherType = core::DispatcherType::kEpoll,
              bool keepAliveEnabled = false,
              uint32_t keepAliveMaxRequests = 100,
              uint32_t keepAliveIdleTimeoutMs = 10000);

    /// @brief 析构时自动调用 stop()
    ~TcpServer();

    /// @brief 启动服务
    /// @retval kOk    正常退出（stop 触发后 accept 循环结束）
    /// @retval kAgain 服务已在运行
    /// @retval kError listen 失败
    core::StatusCode run();

    /// @brief 设置请求处理器
    /// @param handler 处理器 lambda（所有连接共享）
    void setRequestHandler(RequestHandler handler);

    /// @brief 停止服务
    /// @retval kOk    正常停止
    /// @retval kAgain 服务未在运行
    core::StatusCode stop();

  private:
    core::StatusCode acceptConnection();
    void cleanupClosedConnections_();
    void enqueueClosedConnection_(int fd);
    void logMetricsIfNeeded_();

  private:
    int lfd_;
    uint16_t port_;
    std::map<int, std::shared_ptr<TcpConnection>> conns_;
    std::mutex connsMutex_;
    std::vector<int> pendingCloseFds_;
    std::mutex pendingCloseMutex_;
    std::unique_ptr<IOThreadPool> threadPool_;
    core::DispatcherType dispatcherType_;
    bool keepAliveEnabled_;
    uint32_t keepAliveMaxRequests_;
    uint32_t keepAliveIdleTimeoutMs_;
    RequestHandler requestHandler_;
    std::atomic<bool> isRunning_{false};
    std::chrono::steady_clock::time_point lastMetricsLogTime_;
    reactor::observability::MetricsSnapshot lastMetricsSnapshot_;
};
} // namespace reactor::net
