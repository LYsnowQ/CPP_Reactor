#pragma once

#include <atomic>
#include <cstdint>

namespace reactor::observability
{

/// @brief 指标快照，为某一时刻所有计数器的一致性拷贝
///
/// 由 Metrics::snapshot() 返回，用于外部（如 TcpServer）计算
/// 窗口内的增量变化（差值），而非读取瞬时数值。
struct MetricsSnapshot
{
    uint64_t acceptOk = 0;
    uint64_t acceptFail = 0;
    uint64_t activeConnections = 0;
    uint64_t requestsTotal = 0;
    uint64_t responses2xx = 0;
    uint64_t responses4xx = 0;
    uint64_t responses5xx = 0;
    uint64_t bytesRead = 0;
    uint64_t bytesWritten = 0;
    uint64_t requestLatencySamples = 0;
    uint64_t requestLatencyTotalUs = 0;
    uint64_t requestLatencyMaxUs = 0;
};

/// @brief 全局指标采集器（单例模式）
///
/// 所有 public 方法均为线程安全的无锁原子操作（memory_order_relaxed），
/// 可在任意线程（主线程、EventLoop 线程、SqlExecutor 后台线程）调用。
///
/// activeConnections 使用加减计数，在 onConnectionOpened/+1 和
/// onConnectionsClosed/-N 之间维护。
///
/// @thread safe（所有方法均为原子操作）
class Metrics
{
  public:
    /// @brief 获取单例实例
    static Metrics &instance();

    /// @brief accept 成功计数 +1
    void onAcceptOk();

    /// @brief accept 失败计数 +1
    void onAcceptFail();

    /// @brief 活跃连接数 +1（连接建立时调用）
    void onConnectionOpened();

    /// @brief 活跃连接数 -count（连接关闭时调用）
    /// @param count 关闭的连接数（为 0 时直接返回）
    void onConnectionsClosed(uint64_t count);

    /// @brief 请求解析成功计数 +1
    void onRequestParsed();

    /// @brief 根据状态码分类累加 2xx/4xx/5xx 计数
    /// @param statusCode HTTP 状态码
    void onResponseStatus(int statusCode);

    /// @brief 读字节数累加
    /// @param count 本次读取的字节数
    void onBytesRead(uint64_t count);

    /// @brief 写字节数累加
    /// @param count 本次写入的字节数
    void onBytesWritten(uint64_t count);

    /// @brief 记录一次请求延迟
    ///
    /// 同时更新采样数、总延迟和最大延迟三项指标。
    /// 最大延迟使用 CAS 循环更新，保证高并发下不丢失最大值。
    ///
    /// @param latencyUs 本次请求的延迟（微秒）
    void onRequestLatencyUs(uint64_t latencyUs);

    /// @brief 获取当前所有计数器的原子快照
    ///
    /// 快照读取所有 12 个原子计数器并打包到 MetricsSnapshot。
    /// 调用方应将本次快照与上次快照的差值用于计算窗口指标（如 QPS）。
    ///
    /// @return 所有指标的一致拷贝
    MetricsSnapshot snapshot() const;

  private:
    Metrics() = default;

  private:
    std::atomic<uint64_t> acceptOk_{0};
    std::atomic<uint64_t> acceptFail_{0};
    std::atomic<uint64_t> activeConnections_{0};
    std::atomic<uint64_t> requestsTotal_{0};
    std::atomic<uint64_t> responses2xx_{0};
    std::atomic<uint64_t> responses4xx_{0};
    std::atomic<uint64_t> responses5xx_{0};
    std::atomic<uint64_t> bytesRead_{0};
    std::atomic<uint64_t> bytesWritten_{0};
    std::atomic<uint64_t> requestLatencySamples_{0};
    std::atomic<uint64_t> requestLatencyTotalUs_{0};
    std::atomic<uint64_t> requestLatencyMaxUs_{0};
};
} // namespace reactor::observability
