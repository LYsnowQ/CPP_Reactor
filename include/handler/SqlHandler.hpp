#pragma once

#include <memory>

#include "handler/IHttpHandler.hpp"
#include "net/TcpConnection.hpp"
#include "persistence/SqlExecutor.hpp"

namespace reactor::handler
{

/// @brief SQL 查询处理器（异步模式）
///
/// 从 HTTP GET 请求的 URL 参数中提取 SQL（?sql=...），
/// 提交到 SqlExecutor 的后台线程执行，结果通过 EventLoop::post()
/// 投递回所属线程，以 JSON 格式异步回写 HTTP 响应。
///
/// 响应格式：
///   200 — JSON 二维数组 [[col1, col2, ...], ...]
///   400 — {"error":"Missing 'sql' query parameter"}
///   403 — {"error":"Only read-only statements are allowed"}（readOnly 模式）
///   405 — {"error":"Method not allowed"}
///   500 — {"error":"SQL query failed", "message":"..."}
///
/// @param executor SqlExecutor 共享所有权（多个连接/处理器可共用）
/// @param readOnly true 时只允许 SELECT/SHOW/DESCRIBE/EXPLAIN 单条语句
/// @thread handle() 由 EventLoop 线程调用，内部提交到 SqlExecutor 后台线程
/// @warning handle() 返回的 HandlerResult.async = true，TcpConnection 不立即发送响应
class SqlHandler : public IHttpHandler
{
public:
    /// @brief 构造 SQL 查询处理器
    /// @param executor SqlExecutor 共享指针，至少存活到所有未完成的查询返回
    /// @param readOnly 是否启用只读语句护栏（默认 true）
    explicit SqlHandler(std::shared_ptr<persistence::SqlExecutor> executor,
                        bool readOnly = true);

    /// @brief 处理 GET 请求，执行 SQL 查询并异步回写 JSON 结果
    ///
    /// 处理流程：
    ///   1. 校验请求方法（仅 GET）
    ///   2. 从 URL 提取 ?sql= 参数
    ///   3. 标记 result.async = true
    ///   4. 通过 SqlExecutor::submit 提交到后台线程
    ///   5. 后台线程执行 SQL（ThreadLocalSingleConn 管理连接）
    ///   6. 结果通过 EventLoop::post 投递回 EventLoop 线程
    ///   7. 在 EventLoop 线程中调用 TcpConnection::sendAsyncResponse
    net::TcpConnection::HandlerResult handle(
        net::protocol::HttpRequest &req,
        net::TcpConnection &conn) override;

    std::string_view name() const override { return "SqlHandler"; }

private:
    std::shared_ptr<persistence::SqlExecutor> executor_;
    bool readOnly_{true};
};

} // namespace reactor::handler
