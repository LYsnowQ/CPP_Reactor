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

/// @brief SQL 后台执行器（单消费者线程模型）
///
/// 维护一个独立的后台工作线程和一个任务队列。
/// 调用方通过 submit 提交 Job，后台线程依次从队列取出并执行。
///
/// 每个 Job 接收 ThreadLocalSingleConn& 参数，通过 withConnection
/// 获取线程局部连接执行 SQL。
///
/// @param tls ThreadLocalSingleConn（move in，线程局部连接管理器）
/// @thread submit 可跨线程调用（mutex 保护）
class SqlExecutor
{
public:
    /// @brief 后台任务类型
    /// @param tls 线程局部连接管理器（在工作线程中使用）
    using Job = std::function<void(ThreadLocalSingleConn&)>;

    /// @brief 构造执行器（不启动线程）
    /// @param tls 连接管理器（move in）
    explicit SqlExecutor(ThreadLocalSingleConn tls);

    /// @brief 析构时自动调用 stop()
    ~SqlExecutor();

    /// @brief 启动后台工作线程
    ///
    /// 幂等操作：已运行时直接返回。
    /// 创建 worker_ 线程并执行 run_()。
    void start();

    /// @brief 停止后台工作线程
    ///
    /// 幂等操作：未运行时直接返回。
    /// 设置停止标志 → 通知条件变量 → join 工作线程。
    void stop();

    /// @brief 提交 SQL 任务到后台队列
    /// @param job 任务函数
    /// @return true 提交成功，false 提交失败（执行器已停止）
    /// @thread safe（mutex 保护）
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
