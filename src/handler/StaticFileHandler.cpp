#include "handler/StaticFileHandler.hpp"
#include "net/TcpConnection.hpp"
#include "utils/StringUtils.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string_view>
#include <vector>

namespace
{
    // ====================================================================
    // HTML 实体转义
    // ====================================================================
    //
    // 对 5 个 HTML 特殊字符进行转义：& < > " '
    // 在目录索引和错误页面中使用，防止 XSS 和显示错乱。
    std::string htmlEscape_(std::string_view in)
    {
        std::string out;
        out.reserve(in.size() + 32);
        for (char c : in)
        {
            switch (c)
            {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&#39;";
                break;
            default:
                out.push_back(c);
                break;
            }
        }
        return out;
    }

    // ====================================================================
    // Content-Type 检测策略
    // ====================================================================
    //
    // 通过文件扩展名映射到 HTTP Content-Type。
    // 文本类扩展名附加 "charset=utf-8"。
    // 图片类返回标准 image/* 类型。
    // 未识别扩展名统一返回 application/octet-stream。
    std::string contentTypeFromPath_(const std::filesystem::path &p)
    {
        const auto ext = p.extension().string();
        if (ext == ".html" || ext == ".htm")
        {
            return "text/html; charset=utf-8";
        }
        if (ext == ".css")
        {
            return "text/css; charset=utf-8";
        }
        if (ext == ".js")
        {
            return "application/javascript; charset=utf-8";
        }
        if (ext == ".json")
        {
            return "application/json; charset=utf-8";
        }
        if (ext == ".md" || ext == ".txt" || ext == ".hpp" || ext == ".cpp" || ext == ".h" ||
            ext == ".c")
        {
            return "text/plain; charset=utf-8";
        }
        if (ext == ".png")
        {
            return "image/png";
        }
        if (ext == ".jpg" || ext == ".jpeg")
        {
            return "image/jpeg";
        }
        if (ext == ".gif")
        {
            return "image/gif";
        }
        return "application/octet-stream";
    }

    // ====================================================================
    // 路径穿越防护
    // ====================================================================
    //
    // 将 root 和 target 路径拆分为 segment 序列逐段比较。
    // 只有当 target 的每个 segment 均与 root 对应 segment 匹配，
    // 且 target 不比 root 短时返回 true。
    // 单纯用字符串前缀比较会被 "/var/www/../../etc/passwd" 绕过。
    bool isWithinRoot_(const std::filesystem::path &root, const std::filesystem::path &target)
    {
        auto rootIt = root.begin();
        auto targetIt = target.begin();
        for (; rootIt != root.end() && targetIt != target.end(); ++rootIt, ++targetIt)
        {
            if (*rootIt != *targetIt)
            {
                return false;
            }
        }
        return rootIt == root.end();
    }

    // ====================================================================
    // URL 规范化
    // ====================================================================
    //
    // 去除 query string（?）和 fragment（#），
    // 确保以 '/' 开头，空路径映射为 "/"。
    std::string normalizeUrlPath_(std::string rawUrl)
    {
        auto q = rawUrl.find('?');
        if (q != std::string::npos)
        {
            rawUrl.resize(q);
        }
        auto h = rawUrl.find('#');
        if (h != std::string::npos)
        {
            rawUrl.resize(h);
        }
        if (rawUrl.empty())
        {
            rawUrl = "/";
        }
        if (rawUrl.front() != '/')
        {
            rawUrl.insert(rawUrl.begin(), '/');
        }
        return rawUrl;
    }

    // ====================================================================
    // 目录索引 HTML 生成
    // ====================================================================
    //
    // 按"目录在前、文件在后，各自按文件名字母排序"的顺序展示。
    // 父目录链接（..）仅在非根目录时显示。
    // 每个路径段单独 URL 编码，支持中文/特殊字符的目录名。
    std::string buildDirHtml_(const std::filesystem::path &root,
                              const std::filesystem::path &dirPath, std::string_view displayUrl)
    {
        std::vector<std::filesystem::directory_entry> entries;
        for (const auto &entry : std::filesystem::directory_iterator(dirPath))
        {
            entries.push_back(entry);
        }
        // 目录在前，文件在后，各自按文件名字母排序。
        std::sort(entries.begin(), entries.end(),
                  [](const auto &a, const auto &b)
                  {
                      if (a.is_directory() != b.is_directory())
                      {
                          return a.is_directory() > b.is_directory();
                      }
                      return a.path().filename().string() < b.path().filename().string();
                  });

        std::ostringstream oss;
        oss << "<html><head><meta charset=\"utf-8\"><title>Index of " << htmlEscape_(displayUrl)
            << "</title></head><body><h2>Index of " << htmlEscape_(displayUrl) << "</h2><ul>";

        // 父目录链接：仅在非根目录时显示 ".."。
        if (dirPath != root)
        {
            const auto parent = dirPath.parent_path();
            auto relative = std::filesystem::relative(parent, root).generic_string();
            if (relative == ".")
            {
                relative.clear();
            }
            std::string href = "/";
            if (!relative.empty())
            {
                href += relative + "/";
            }
            oss << "<li><a href=\"" << htmlEscape_(href) << "\">..</a></li>";
        }

        for (const auto &entry : entries)
        {
            const auto name = entry.path().filename().string();
            auto relative = std::filesystem::relative(entry.path(), root).generic_string();
            std::string encoded;
            std::stringstream ss(relative);
            std::string seg;
            while (std::getline(ss, seg, '/'))
            {
                if (seg.empty())
                {
                    continue;
                }
                if (!encoded.empty())
                {
                    encoded += "/";
                }
                encoded += reactor::utils::urlEncodePathComponent(seg);
            }
            std::string href = "/" + encoded;
            if (entry.is_directory())
            {
                href += "/";
            }
            oss << "<li><a href=\"" << htmlEscape_(href) << "\">" << htmlEscape_(name);
            if (entry.is_directory())
            {
                oss << "/";
            }
            oss << "</a></li>";
        }
        oss << "</ul></body></html>";
        return oss.str();
    }
} // namespace

// ====================================================================
// 构造
// ====================================================================
//
// 构造时立即对 root 路径做 weakly_canonical 规范化并保存。
// 后续所有请求的路径均以此规范化路径为基准进行比较，
// 避免因符号链接或相对路径导致的路径穿越漏洞。
reactor::handler::StaticFileHandler::StaticFileHandler(std::filesystem::path root)
    : root_(std::filesystem::weakly_canonical(std::move(root)))
{}

// ====================================================================
// 请求处理主流程（同步模式）
// ====================================================================
//
// 1. 方法校验：仅放行 GET，其余返回 405。
// 2. URL 规范化：去除 query string 和 fragment。
// 3. 特殊路径 /healthz 直接返回 "ok\n"。
// 4. 路径解码后拼接到 root_ 形成完整路径。
// 5. 路径穿越保护：weakly_canonical 规范化后逐段比较。
// 6. 文件/目录存在性检查。
// 7. 目录 → buildDirHtml_ 生成 HTML 目录索引。
// 8. 文件 → ifstream 二进制读取，contentTypeFromPath_ 设置 MIME 类型。
reactor::net::TcpConnection::HandlerResult
reactor::handler::StaticFileHandler::handle(
    reactor::net::protocol::HttpRequest &req,
    reactor::net::TcpConnection &)
{
    reactor::net::TcpConnection::HandlerResult result;
    const auto method = req.getMethed();
    if (method != "GET" && method != "get")
    {
        result.statusCode = 405;
        result.reasonPhrase = "Method Not Allowed";
        result.body = "Only GET is supported.\n";
        return result;
    }
    auto urlPath = normalizeUrlPath_(req.getUrl());
    if (urlPath == "/healthz")
    {
        result.statusCode = 200;
        result.reasonPhrase = "OK";
        result.body = "ok\n";
        result.contentType = "text/plain; charset=utf-8";
        return result;
    }

    const auto decodedPath = reactor::utils::urlDecode(urlPath);
    auto relative = std::filesystem::path(decodedPath).relative_path();
    // 防止通过符号链接绕过 staticRoot 限制。
    auto candidate = std::filesystem::weakly_canonical(root_ / relative);
    // 路径遍历防护：使用规范化后的路径逐段比较。
    if (!isWithinRoot_(root_, candidate))
    {
        result.statusCode = 403;
        result.reasonPhrase = "Forbidden";
        result.body = "Path is outside of static root.\n";
        return result;
    }

    std::error_code ec;
    const bool exists = std::filesystem::exists(candidate, ec);
    if (ec || !exists)
    {
        result.statusCode = 404;
        result.reasonPhrase = "Not Found";
        result.body = "Not Found\n";
        return result;
    }

    if (std::filesystem::is_directory(candidate, ec))
    {
        if (ec)
        {
            result.statusCode = 500;
            result.reasonPhrase = "Internal Server Error";
            result.body = "Failed to list directory.\n";
            return result;
        }
        result.statusCode = 200;
        result.reasonPhrase = "OK";
        result.contentType = "text/html; charset=utf-8";
        result.body = buildDirHtml_(root_, candidate, urlPath);
        return result;
    }

    std::ifstream ifs(candidate, std::ios::binary);
    if (!ifs.is_open())
    {
        result.statusCode = 500;
        result.reasonPhrase = "Internal Server Error";
        result.body = "Failed to open file.\n";
        return result;
    }
    std::string body((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    result.statusCode = 200;
    result.reasonPhrase = "OK";
    result.body = std::move(body);
    result.contentType = contentTypeFromPath_(candidate);
    return result;
}
