#pragma once
#include "persistence/SqlTypes.hpp"
#include "persistence/ISqlConnection.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <chrono>

namespace reactor::repository::sql
{

/// @brief SQL 查询执行结果
///
/// @param rows      查询结果集（ok=false 时无效）
/// @param elapsedMs 执行耗时（毫秒）
struct SqlTaskResult
{
    persistence::Result<persistence::SqlRows> rows;
    int64_t elapsedMs{0};
};

/// @brief 事件循环回调投递接口（抽象）
///
/// 用于 SqlTask 在后台线程执行完成后将结果投递回事件循环线程。
/// 当前预留为设计接口，具体实现由 EventLoop::post 提供。
/// @thread safe（post 可跨线程调用）
class ILoopPoster
{
public:
    virtual ~ILoopPoster() = default;

    /// @brief 投递回调到目标线程异步执行
    /// @param fn 回调函数
    virtual void post(std::function<void()> fn) = 0;
};

/// @brief SQL 任务描述结构体（设计桩）
///
/// 一次完整的 SQL 查询任务描述，包含：
///   - id：任务唯一标识
///   - traceId：追踪 ID（用于日志串联）
///   - deadline：超时时间点
///   - run：实际执行 SQL 查询的函数
///   - onDone：查询完成后的回调（通过 loopPoster 投递回目标线程）
///   - loopPoster：回调投递器
///
/// 当前为设计桩，未被主路径使用。
/// 后续可扩展为完整的异步任务队列 + 超时取消机制。
struct SqlTask
{
    uint64_t id{0};
    std::string traceId;
    std::chrono::steady_clock::time_point deadline{};
    std::function<persistence::Result<persistence::SqlRows>(persistence::ISqlConnection &)> run;
    std::function<void(SqlTaskResult)> onDone;
    std::shared_ptr<ILoopPoster> loopPoster;
};

} // namespace reactor::repository::sql
