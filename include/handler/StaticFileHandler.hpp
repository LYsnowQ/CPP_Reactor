#pragma once

#include <filesystem>

#include "handler/IHttpHandler.hpp"
#include "net/TcpConnection.hpp"

namespace reactor::handler
{

/// @brief 静态文件服务处理器
///
/// 提供以下功能：
///   1. GET 请求返回对应文件内容（根据扩展名自动设置 Content-Type）
///   2. 访问目录时返回目录索引 HTML 页面（目录文件在前，文件在后，按字母排序）
///   3. /healthz 路径返回 "ok\n"（健康检查端点）
///   4. 路径穿越保护：验证请求路径是否在 staticRoot 范围内
///
/// 响应状态码：
///   200 — 正常返回文件或目录列表
///   403 — 请求路径超出 staticRoot（路径穿越）
///   404 — 文件或目录不存在
///   405 — 非 GET 请求
///   500 — 文件读取或目录遍历失败
///
/// @param root 静态文件根目录（构造时通过 weakly_canonical 规范化）
/// @thread 仅由所属 EventLoop 线程调用
class StaticFileHandler : public IHttpHandler
{
public:
    /// @brief 构造静态文件处理器
    /// @param root 静态文件根目录路径
    explicit StaticFileHandler(std::filesystem::path root);

    /// @brief 处理 GET 请求，返回静态文件或目录索引
    ///
    /// 处理流程：
    ///   1. 校验请求方法（仅 GET）
    ///   2. 规范化 URL 路径（去 query/fragment）
    ///   3. 路径穿越检查
    ///   4. 检查文件/目录是否存在
    ///   5. 目录 → 生成目录索引 HTML
    ///   6. 文件 → 读取全部内容并设置 Content-Type
    net::TcpConnection::HandlerResult handle(
        net::protocol::HttpRequest &req,
        net::TcpConnection &conn) override;

    std::string_view name() const override { return "StaticFileHandler"; }

private:
    std::filesystem::path root_;
};
} // namespace reactor::handler
