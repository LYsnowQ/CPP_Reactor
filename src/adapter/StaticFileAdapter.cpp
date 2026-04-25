#include "StaticFileAdapter.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string_view>
#include <vector>

namespace
{
std::string toHex_(unsigned char c)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.push_back(kHex[(c >> 4) & 0x0F]);
    out.push_back(kHex[c & 0x0F]);
    return out;
}

std::string urlEncodePathComponent_(std::string_view in)
{
    std::string out;
    for(unsigned char c : in)
    {
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
           c == '~')
        {
            out.push_back(static_cast<char>(c));
        }
        else
        {
            out.push_back('%');
            out += toHex_(c);
        }
    }
    return out;
}

int hexValue_(char c)
{
    if(c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if(c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if(c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

std::string urlDecode_(std::string_view in)
{
    std::string out;
    out.reserve(in.size());
    for(size_t i = 0; i < in.size(); ++i)
    {
        const char c = in[i];
        if(c == '%' && i + 2 < in.size())
        {
            const int hi = hexValue_(in[i + 1]);
            const int lo = hexValue_(in[i + 2]);
            if(hi >= 0 && lo >= 0)
            {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        if(c == '+')
        {
            out.push_back(' ');
            continue;
        }
        out.push_back(c);
    }
    return out;
}

std::string htmlEscape_(std::string_view in)
{
    std::string out;
    out.reserve(in.size() + 32);
    for(char c : in)
    {
        switch(c)
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

std::string contentTypeFromPath_(const std::filesystem::path& p)
{
    const auto ext = p.extension().string();
    if(ext == ".html" || ext == ".htm")
    {
        return "text/html; charset=utf-8";
    }
    if(ext == ".css")
    {
        return "text/css; charset=utf-8";
    }
    if(ext == ".js")
    {
        return "application/javascript; charset=utf-8";
    }
    if(ext == ".json")
    {
        return "application/json; charset=utf-8";
    }
    if(ext == ".md" || ext == ".txt" || ext == ".hpp" || ext == ".cpp" || ext == ".h" || ext == ".c")
    {
        return "text/plain; charset=utf-8";
    }
    if(ext == ".png")
    {
        return "image/png";
    }
    if(ext == ".jpg" || ext == ".jpeg")
    {
        return "image/jpeg";
    }
    if(ext == ".gif")
    {
        return "image/gif";
    }
    return "application/octet-stream";
}

bool isWithinRoot_(const std::filesystem::path& root, const std::filesystem::path& target)
{
    auto rootIt = root.begin();
    auto targetIt = target.begin();
    for(; rootIt != root.end() && targetIt != target.end(); ++rootIt, ++targetIt)
    {
        if(*rootIt != *targetIt)
        {
            return false;
        }
    }
    return rootIt == root.end();
}

std::string normalizeUrlPath_(std::string rawUrl)
{
    auto q = rawUrl.find('?');
    if(q != std::string::npos)
    {
        rawUrl.resize(q);
    }
    auto h = rawUrl.find('#');
    if(h != std::string::npos)
    {
        rawUrl.resize(h);
    }
    if(rawUrl.empty())
    {
        rawUrl = "/";
    }
    if(rawUrl.front() != '/')
    {
        rawUrl.insert(rawUrl.begin(), '/');
    }
    return rawUrl;
}

std::string buildDirHtml_(const std::filesystem::path& root, const std::filesystem::path& dirPath, std::string_view displayUrl)
{
    std::vector<std::filesystem::directory_entry> entries;
    for(const auto& entry : std::filesystem::directory_iterator(dirPath))
    {
        entries.push_back(entry);
    }
    std::sort(
        entries.begin(),
        entries.end(),
        [](const auto& a, const auto& b)
        {
            if(a.is_directory() != b.is_directory())
            {
                return a.is_directory() > b.is_directory();
            }
            return a.path().filename().string() < b.path().filename().string();
        });

    std::ostringstream oss;
    oss << "<html><head><meta charset=\"utf-8\"><title>Index of " << htmlEscape_(displayUrl)
        << "</title></head><body><h2>Index of " << htmlEscape_(displayUrl) << "</h2><ul>";

    if(dirPath != root)
    {
        const auto parent = dirPath.parent_path();
        auto relative = std::filesystem::relative(parent, root).generic_string();
        if(relative == ".")
        {
            relative.clear();
        }
        std::string href = "/";
        if(!relative.empty())
        {
            href += relative + "/";
        }
        oss << "<li><a href=\"" << htmlEscape_(href) << "\">..</a></li>";
    }

    for(const auto& entry : entries)
    {
        const auto name = entry.path().filename().string();
        auto relative = std::filesystem::relative(entry.path(), root).generic_string();
        std::string encoded;
        std::stringstream ss(relative);
        std::string seg;
        while(std::getline(ss, seg, '/'))
        {
            if(seg.empty())
            {
                continue;
            }
            if(!encoded.empty())
            {
                encoded += "/";
            }
            encoded += urlEncodePathComponent_(seg);
        }
        std::string href = "/" + encoded;
        if(entry.is_directory())
        {
            href += "/";
        }
        oss << "<li><a href=\"" << htmlEscape_(href) << "\">" << htmlEscape_(name);
        if(entry.is_directory())
        {
            oss << "/";
        }
        oss << "</a></li>";
    }
    oss << "</ul></body></html>";
    return oss.str();
}
}

reactor::net::TcpConnection::RequestHandler
reactor::net::adapter::StaticFileAdapter::createHandler(const std::filesystem::path& root)
{
    const auto staticRoot = std::filesystem::weakly_canonical(root);
    return [staticRoot](reactor::net::protocol::HttpRequest& req) -> reactor::net::TcpConnection::HandlerResult
    {
        reactor::net::TcpConnection::HandlerResult result;
        const auto method = req.getMethed();
        if(method != "GET" && method != "get")
        {
            result.statusCode = 405;
            result.reasonPhrase = "Method Not Allowed";
            result.body = "Only GET is supported.\n";
            return result;
        }
        auto urlPath = normalizeUrlPath_(req.getUrl());
        if(urlPath == "/healthz")
        {
            result.statusCode = 200;
            result.reasonPhrase = "OK";
            result.body = "ok\n";
            result.contentType = "text/plain; charset=utf-8";
            return result;
        }

        const auto decodedPath = urlDecode_(urlPath);
        auto relative = std::filesystem::path(decodedPath).relative_path();
        auto candidate = std::filesystem::weakly_canonical(staticRoot / relative);
        if(!isWithinRoot_(staticRoot, candidate))
        {
            result.statusCode = 403;
            result.reasonPhrase = "Forbidden";
            result.body = "Path is outside of static root.\n";
            return result;
        }

        std::error_code ec;
        const bool exists = std::filesystem::exists(candidate, ec);
        if(ec || !exists)
        {
            result.statusCode = 404;
            result.reasonPhrase = "Not Found";
            result.body = "Not Found\n";
            return result;
        }

        if(std::filesystem::is_directory(candidate, ec))
        {
            if(ec)
            {
                result.statusCode = 500;
                result.reasonPhrase = "Internal Server Error";
                result.body = "Failed to list directory.\n";
                return result;
            }
            result.statusCode = 200;
            result.reasonPhrase = "OK";
            result.contentType = "text/html; charset=utf-8";
            result.body = buildDirHtml_(staticRoot, candidate, urlPath);
            return result;
        }

        std::ifstream ifs(candidate, std::ios::binary);
        if(!ifs.is_open())
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
    };
}
