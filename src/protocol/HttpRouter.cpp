#include "protocol/HttpRouter.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace reactor::net::protocol
{
    // -----------------------------------------------------------------------
    // 路由注册
    // -----------------------------------------------------------------------

    HttpRouter &HttpRouter::addRoute(std::string method, std::string pattern,
                                     std::shared_ptr<reactor::handler::IHttpHandler> handler)
    {
        routes_.push_back({std::move(method), std::move(pattern), false, std::move(handler)});
        return *this;
    }

    HttpRouter &HttpRouter::get(std::string pattern,
                                std::shared_ptr<reactor::handler::IHttpHandler> handler)
    {
        return addRoute("GET", std::move(pattern), std::move(handler));
    }

    HttpRouter &HttpRouter::post(std::string pattern,
                                 std::shared_ptr<reactor::handler::IHttpHandler> handler)
    {
        return addRoute("POST", std::move(pattern), std::move(handler));
    }

    HttpRouter &HttpRouter::put(std::string pattern,
                                std::shared_ptr<reactor::handler::IHttpHandler> handler)
    {
        return addRoute("PUT", std::move(pattern), std::move(handler));
    }

    HttpRouter &HttpRouter::del(std::string pattern,
                                std::shared_ptr<reactor::handler::IHttpHandler> handler)
    {
        return addRoute("DELETE", std::move(pattern), std::move(handler));
    }

    HttpRouter &HttpRouter::addPrefix(std::string prefix,
                                      std::shared_ptr<reactor::handler::IHttpHandler> handler)
    {
        routes_.push_back({"", std::move(prefix), true, std::move(handler)});
        return *this;
    }

    // -----------------------------------------------------------------------
    // 路径拆分
    // -----------------------------------------------------------------------

    std::vector<std::string> HttpRouter::splitPath_(std::string_view path)
    {
        std::vector<std::string> segments;

        // 去掉 query string（第一个 ? 之后的部分）
        auto qpos = path.find('?');
        std::string_view stripped = (qpos == std::string_view::npos)
                                        ? path
                                        : path.substr(0, qpos);

        size_t start = 0;
        while (start < stripped.size())
        {
            // 跳过 '/'
            while (start < stripped.size() && stripped[start] == '/')
            {
                ++start;
            }
            if (start >= stripped.size())
            {
                break;
            }

            auto end = stripped.find('/', start);
            if (end == std::string_view::npos)
            {
                segments.emplace_back(stripped.substr(start));
                break;
            }
            segments.emplace_back(stripped.substr(start, end - start));
            start = end + 1;
        }

        return segments;
    }

    // -----------------------------------------------------------------------
    // 段匹配（支持 :param 占位符）
    // -----------------------------------------------------------------------

    std::optional<std::unordered_map<std::string, std::string>>
    HttpRouter::matchSegments_(const std::vector<std::string> &patternSegs,
                               const std::vector<std::string> &urlSegs)
    {
        if (patternSegs.size() != urlSegs.size())
        {
            return std::nullopt;
        }

        std::unordered_map<std::string, std::string> params;
        for (size_t i = 0; i < patternSegs.size(); ++i)
        {
            const auto &pSeg = patternSegs[i];
            const auto &uSeg = urlSegs[i];

            if (!pSeg.empty() && pSeg[0] == ':')
            {
                // 参数段
                params[pSeg.substr(1)] = uSeg;
            }
            else if (pSeg != uSeg)
            {
                return std::nullopt;
            }
        }
        return params;
    }

    // -----------------------------------------------------------------------
    // 分派
    // -----------------------------------------------------------------------

    TcpConnection::HandlerResult HttpRouter::dispatch(HttpRequest &req, TcpConnection &conn)
    {
        const auto url = req.getUrl();
        const auto method = req.getMethed();
        const auto urlSegs = splitPath_(url);

        // --- 第一轮：精确匹配（非前缀）---
        for (const auto &entry : routes_)
        {
            if (entry.isPrefix)
            {
                continue;
            }

            // 方法校验
            if (!entry.method.empty() &&
                !(entry.method.size() == method.size() &&
                  std::equal(entry.method.begin(), entry.method.end(), method.begin(),
                             [](unsigned char a, unsigned char b) {
                                 return std::tolower(a) == std::tolower(b);
                             })))
            {
                continue;
            }

            const auto patternSegs = splitPath_(entry.pattern);
            auto params = matchSegments_(patternSegs, urlSegs);
            if (params.has_value())
            {
                // 将路径参数注入请求对象
                for (auto &[k, v] : *params)
                {
                    req.setPathParam(k, std::move(v));
                }
                return entry.handler->handle(req, conn);
            }
        }

        // --- 第二轮：前缀匹配 ---
        for (const auto &entry : routes_)
        {
            if (!entry.isPrefix)
            {
                continue;
            }

            if (!entry.method.empty() &&
                !(entry.method.size() == method.size() &&
                  std::equal(entry.method.begin(), entry.method.end(), method.begin(),
                             [](unsigned char a, unsigned char b) {
                                 return std::tolower(a) == std::tolower(b);
                             })))
            {
                continue;
            }

            if (url.find(entry.pattern) == 0)
            {
                return entry.handler->handle(req, conn);
            }
        }

        // --- 无匹配：404 ---
        TcpConnection::HandlerResult result;
        result.statusCode = 404;
        result.reasonPhrase = "Not Found";
        result.body = "Not Found\n";
        return result;
    }

} // namespace reactor::net::protocol
