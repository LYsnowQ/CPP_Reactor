#pragma once
#include "persistence/SqlExecutor.hpp"
#include "persistence/SqlTypes.hpp"
#include <functional>
#include <string>
#include <vector>

namespace reactor::repository::sql
{

/// @brief 用户数据仓储，封装用户相关的数据库操作
///
/// 当前提供基于 userId 的异步查询能力，查询通过 SqlExecutor 提交到
/// 后台线程执行，结果通过 Done 回调返回。回调在 SqlExecutor 的
/// 后台线程中执行，调用方需自行通过 EventLoop::post 切回目标线程。
///
/// @param executor SqlExecutor 引用（非拥有），生命周期由调用方保证
/// @thread 非线程安全，SqlRepository 实例应单线程使用
class SqlRepository
{
public:
    /// @brief 异步查询完成回调
    /// @param result 查询结果（ok=true 时 value 有效，ok=false 时 err 描述错误）
    using Done = std::function<void(persistence::Result<persistence::SqlRows>)>;

    /// @brief 构造 SqlRepository
    /// @param executor SqlExecutor 引用（调用方保证其生命周期长于本对象）
    explicit SqlRepository(persistence::SqlExecutor &executor);

    /// @brief 根据 userId 异步查询用户信息
    ///
    /// 提交 SQL 查询到后台线程执行：
    ///   SELECT id, name, status FROM users WHERE id = ?
    ///
    /// 查询结果通过 done 回调返回。
    ///
    /// @param userId 目标用户 ID
    /// @param done   查询完成回调（在 SqlExecutor 后台线程执行）
    /// @return true  提交成功
    /// @return false 提交失败（如 SqlExecutor 已停止）
    /// @thread safe（内部委托给 executor_.submit）
    /// @warning done 在后台线程执行，不要在其中直接操作 TcpConnection 或 Channel
    bool findUserByIdAsync(int64_t userId, Done done);

private:
    persistence::SqlExecutor &executor_;
};
} // namespace reactor::repository::sql
