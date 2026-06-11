#pragma once
#include "persistence/ISqlConnection.hpp"
#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace reactor::persistence
{

/// @brief 连接管理选项
///
/// @param pingBeforeUse        每次使用前是否执行 ping 检测（默认 true）
/// @param maxConnLifetimeMs    连接最大生命周期（默认 30 分钟）
/// @param maxConnIdleMs        连接最大空闲时间（默认 60 秒）
struct ThreadLocalConnOptions
{
    bool pingBeforeUse{true};
    uint32_t maxConnLifetimeMs{30 * 60 * 1000};
    uint32_t maxConnIdleMs{60 * 1000};
};

/// @brief 线程局部单连接管理器
///
/// 每个线程维护一个独立的 ISqlConnection 实例（thread_local），
/// 通过 withConnection 提供"按需创建 → 自动验证 → 使用 → 回收"的完整生命周期管理。
///
/// 连接生命周期策略：
///   - 按需创建：首次 withConnection 时调用 factory_ 创建
///   - 自动验证：每次使用前检查连接状态，失效时重建
///   - 空闲超时：超过 maxConnIdleMs 未使用自动重建
///   - 最大生命周期：超过 maxConnLifetimeMs 自动重建
///   - 脏连接标记：ping 失败时标记 broken，下次使用时重建
///
/// @param cfg     连接管理选项
/// @param factory 连接工厂（创建 ISqlConnection 实例）
/// @thread 支持多线程（thread_local 隔离，各线程独立实例）
class ThreadLocalSingleConn
{
  public:
    using ConnFactory = std::function<std::unique_ptr<ISqlConnection>()>;

    /// @brief 构造连接管理器
    ThreadLocalSingleConn(ThreadLocalConnOptions cfg, ConnFactory factory);

    /// @brief 获取连接并执行操作
    ///
    /// 内部流程：
    ///   1. ensureConnected_() — 按需创建/重建连接
    ///   2. 更新 lastUsed 时间戳
    ///   3. 执行 fn(*conn)
    ///
    /// @tparam F 操作函数类型（接收 ISqlConnection& 引用）
    /// @param fn 要执行的操作
    /// @return fn 的返回值
    /// @throw std::runtime_error 连接失败或 ping 失败
    template <class F>
    auto withConnection(F &&fn) -> std::invoke_result_t<F, ISqlConnection &>;

    /// @brief 清理当前线程的连接资源
    ///
    /// 关闭并释放连接，重置 broken 标记。
    /// 通常由 SqlExecutor 在工作线程退出时调用。
    void cleanupCurrentThread();

  private:
    struct Slot
    {
        std::unique_ptr<ISqlConnection> conn;
        std::chrono::steady_clock::time_point created{};
        std::chrono::steady_clock::time_point lastUsed{};
        bool broken{false};
    };

    Slot &slot_();
    void ensureConnected_();

  private:
    static thread_local Slot tls_;
    ThreadLocalConnOptions cfg_;
    ConnFactory factory_;
};

template <class F>
auto ThreadLocalSingleConn::withConnection(F &&fn) -> std::invoke_result_t<F, ISqlConnection &>
{
    ensureConnected_();
    auto &s = slot_();
    s.lastUsed = std::chrono::steady_clock::now();
    return fn(*s.conn);
}
} // namespace reactor::persistence
