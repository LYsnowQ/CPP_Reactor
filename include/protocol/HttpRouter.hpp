#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <optional>

#include "handler/IHttpHandler.hpp"
#include "net/TcpConnection.hpp"
#include "protocol/HttpRequest.hpp"

namespace reactor::net::protocol
{
    // 路由条目
    struct RouteEntry
    {
        std::string method;   // HTTP 方法，空串表示匹配任意方法
        std::string pattern;  // 路径模式，如 "/api/users/:id"
        bool isPrefix{false}; // true = 前缀匹配, false = 精确匹配
        std::shared_ptr<reactor::handler::IHttpHandler> handler;
    };

    class HttpRouter
    {
    public:
        // 注册路由（通用接口）
        HttpRouter &addRoute(std::string method, std::string pattern,
                             std::shared_ptr<reactor::handler::IHttpHandler> handler);

        // 便捷注册
        HttpRouter &get(std::string pattern,
                        std::shared_ptr<reactor::handler::IHttpHandler> handler);
        HttpRouter &post(std::string pattern,
                         std::shared_ptr<reactor::handler::IHttpHandler> handler);
        HttpRouter &put(std::string pattern,
                        std::shared_ptr<reactor::handler::IHttpHandler> handler);
        HttpRouter &del(std::string pattern,
                        std::shared_ptr<reactor::handler::IHttpHandler> handler);

        // 前缀匹配注册：URL 以 prefix 开头即匹配（用于挂载静态目录）
        HttpRouter &addPrefix(std::string prefix,
                              std::shared_ptr<reactor::handler::IHttpHandler> handler);

        // 分派：根据请求找到匹配的处理器
        // 无匹配时返回 404
        TcpConnection::HandlerResult dispatch(HttpRequest &req, TcpConnection &conn);

    private:
        // 将路径按 '/' 拆分为 segments（跳过连续斜杠，过滤空段）
        static std::vector<std::string> splitPath_(std::string_view path);

        // 尝试精确模式匹配，成功返回路径参数
        static std::optional<std::unordered_map<std::string, std::string>>
        matchSegments_(const std::vector<std::string> &patternSegs,
                       const std::vector<std::string> &urlSegs);

        std::vector<RouteEntry> routes_;
    };

} // namespace reactor::net::protocol
