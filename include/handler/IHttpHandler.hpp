#pragma once

#include <memory>
#include <string_view>

#include "net/TcpConnection.hpp"
#include "protocol/HttpRequest.hpp"

namespace reactor::handler
{

/// @brief 请求处理器抽象接口
///
/// 所有具体处理器（StaticFileHandler、SqlHandler 等）继承此类。
/// 通过 HttpRouter 注册并分派，每个请求对应一次 handle 调用。
///
/// handle() 返回的 HandlerResult 支持同步和异步两种模式：
///   - 同步模式：在 handle() 返回前填写完整的 HandlerResult
///   - 异步模式：设置 result.async = true，通过 TcpConnection::sendAsyncResponse 回写
///
/// @warning handle() 必须返回 HandlerResult，即使是异步模式也须返回 {async=true}
/// @warning handle() 内部不应长时间阻塞（IO 操作应通过 SqlExecutor 提交到后台线程）
class IHttpHandler
{
public:
    virtual ~IHttpHandler() = default;

    /// @brief 处理 HTTP 请求并返回响应结果
    /// @param req  解析后的 HTTP 请求对象
    /// @param conn 对应的 TCP 连接（用于异步回写或读取连接信息）
    /// @return HandlerResult（同步模式直接包含响应体，异步模式标记 async=true）
    virtual net::TcpConnection::HandlerResult handle(
        net::protocol::HttpRequest &req,
        net::TcpConnection &conn) = 0;

    /// @brief 返回处理器名称，用于日志和监控
    /// @return 处理器标识字符串（如 "StaticFileHandler"、"SqlHandler"）
    virtual std::string_view name() const { return "IHttpHandler"; }
};

} // namespace reactor::handler
