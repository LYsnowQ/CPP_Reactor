#include "protocol/HttpRequest.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <cctype>
#include <charconv>
#include <system_error>
#include <sys/types.h>

namespace reactor::net::protocol
{
    HttpRequest::HttpRequest(base::Buffer *dataPackage)
        : data_(dataPackage), method_(std::string()), url_(std::string()), version_(std::string()),
          curState_(HttpRequestState::kIdle)
    {
    }

    HttpRequest::ParseResult HttpRequest::parseRequest(base::Buffer *data,
                                                       std::unique_ptr<HttpRequest> *inFlight)
    {
        if (!data)
        {
            return {core::StatusCode::kInvalid, nullptr, "null request buffer", false};
        }
        if (data->readableBytes() == 0)
        {
            return {core::StatusCode::kAgain, nullptr, "empty request", false};
        }
        std::unique_ptr<HttpRequest> local;
        std::unique_ptr<HttpRequest> *holder = inFlight ? inFlight : &local;
        if (!(*holder))
        {
            holder->reset(new HttpRequest(data));
        }
        (*holder)->data_ = data;
        auto &res = *holder;

        const auto code = res->parse_();
        if (code != core::StatusCode::kOk)
        {
            switch (res->curState_)
            {
            case (HttpRequestState::kParseReqLineFailed):
                return {code, nullptr, "invalid request line", false};
            case (HttpRequestState::kParseReqHeadersFailed):
                return {code, nullptr, "invalid headers", false};
            case (HttpRequestState::kParseReqBodyFailed):
                return {code, nullptr, res->bodyTooLarge_ ? "body too large" : "invalid body",
                        res->bodyTooLarge_};
            default:
                return {code, nullptr, "incomplete request", false};
            }
        }
        return {core::StatusCode::kOk, std::move(*holder), "ok", false};
    }

    std::string HttpRequest::getMethod()
    {
        if (method_.length())
        {
            return method_;
        }
        return "";
    }

    std::string HttpRequest::getUrl()
    {
        if (url_.length())
        {
            return url_;
        }
        return "";
    }

    std::string HttpRequest::version()
    {
        if (version_.length())
        {
            return version_;
        }
        return "";
    }

    std::vector<std::pair<std::string, std::string>> HttpRequest::getHeader()
    {
        return headers_;
    }

    std::string HttpRequest::getBody()
    {
        return body_;
    }

    core::StatusCode HttpRequest::parse_()
    {
        if (curState_ == HttpRequestState::kParseDone)
        {
            return core::StatusCode::kOk;
        }
        if (curState_ == HttpRequestState::kIdle || curState_ == HttpRequestState::kParseReqLine)
        {
            const auto code = parseLine_();
            if (code != core::StatusCode::kOk)
            {
                return code;
            }
            curState_ = HttpRequestState::kParseReqHeaders;
        }

        if (curState_ == HttpRequestState::kParseReqHeaders)
        {
            const auto code = parseHead_();
            if (code != core::StatusCode::kOk)
            {
                return code;
            }
            curState_ = HttpRequestState::kParseReqBody;
        }

        if (curState_ == HttpRequestState::kParseReqBody)
        {
            return parseBody_();
        }
        return core::StatusCode::kInvalid;
    }

    core::StatusCode HttpRequest::parseLine_()
    {
        curState_ = HttpRequestState::kParseReqLine;

        auto lineEnd = data_->find("\r\n");

        if (lineEnd == std::string::npos)
        {
            return core::StatusCode::kAgain;
        }

        const auto requestLineView = data_->getStringView(lineEnd);
        std::string requestLine(requestLineView.data(), requestLineView.size());

        const auto firstSpace = requestLine.find(' ');
        if (firstSpace == std::string::npos)
        {
            curState_ = HttpRequestState::kParseReqLineFailed;
            return core::StatusCode::kInvalid;
        }

        const auto secondSpace = requestLine.find(' ', firstSpace + 1);
        if (secondSpace == std::string::npos)
        {
            curState_ = HttpRequestState::kParseReqLineFailed;
            return core::StatusCode::kInvalid;
        }

        method_ = requestLine.substr(0, firstSpace);
        url_ = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
        version_ = requestLine.substr(secondSpace + 1);

        if (method_.empty() || url_.empty() || version_.empty())
        {
            curState_ = HttpRequestState::kParseReqLineFailed;
            return core::StatusCode::kInvalid;
        }

        data_->retrieve(lineEnd + 2);
        return core::StatusCode::kOk;
    }

    core::StatusCode HttpRequest::parseHead_()
    {
        curState_ = HttpRequestState::kParseReqHeaders;
        if (data_->find("\r\n\r\n") == std::string::npos)
        {
            return core::StatusCode::kAgain;
        }

        while (true)
        {
            auto index = data_->find("\r\n");
            if (index == std::string::npos)
            {
                return core::StatusCode::kAgain;
            }

            if (index == 0)
            {
                data_->retrieve(2);
                break;
            }

            std::string_view subView = data_->getStringView(index);
            std::string sub(subView.data(), subView.size());
            data_->retrieve(index + 2);

            // 分割请求头键值
            std::string::size_type subpos = sub.find(": ");
            if (subpos == std::string::npos)
            {
                curState_ = HttpRequestState::kParseReqHeadersFailed;
                return core::StatusCode::kInvalid;
            }

            std::string key = sub.substr(0, subpos);
            std::string value = sub.substr(subpos + 2);
            /*由于头部请求策略问题，其中有些参数多个值并非有单一拆分原则，具体如下：
             * 1.大部分请求头均是逗号隔离多个值。
             * 2.Set-Cookie由于内容支持逗号则其策略为多个值存储多次，且内部是使用；来区分的
             * 3.每个Cookie头只包含一组name = value
             * 基于以上情况，这里不做拆分，只做整行存储,具体情况交给上层处理
            while(subpos<sub.size()){
                subpos = sub.find(",",subpos)
                }
            */
            headers_.emplace_back(key, value);
        }
        return core::StatusCode::kOk;
    }

    core::StatusCode HttpRequest::parseBody_()
    {
        curState_ = HttpRequestState::kParseReqBody;
        bodyTooLarge_ = false;

        const auto contentLengthOpt = findHeader_("Content-Length");
        if (!contentLengthOpt.has_value())
        {
            body_.clear();
            curState_ = HttpRequestState::kParseDone;
            return core::StatusCode::kOk;
        }

        size_t contentLength = 0;
        const auto &contentLengthValue = contentLengthOpt.value();
        const auto trimmedBegin = contentLengthValue.find_first_not_of(" \t");
        if (trimmedBegin == std::string::npos)
        {
            curState_ = HttpRequestState::kParseReqBodyFailed;
            return core::StatusCode::kInvalid;
        }
        const auto trimmedEnd = contentLengthValue.find_last_not_of(" \t");
        const auto trimmed = contentLengthValue.substr(trimmedBegin, trimmedEnd - trimmedBegin + 1);
        const auto begin = trimmed.data();
        const auto end = trimmed.data() + trimmed.size();
        const auto res = std::from_chars(begin, end, contentLength);
        if (res.ec != std::errc() || res.ptr != end)
        {
            curState_ = HttpRequestState::kParseReqBodyFailed;
            return core::StatusCode::kInvalid;
        }

        if (contentLength > kMaxBodyBytes)
        {
            bodyTooLarge_ = true;
            curState_ = HttpRequestState::kParseReqBodyFailed;
            return core::StatusCode::kInvalid;
        }

        if (data_->readableBytes() < contentLength)
        {
            return core::StatusCode::kAgain;
        }

        body_ = data_->retrieveAsString(contentLength);
        curState_ = HttpRequestState::kParseDone;
        return core::StatusCode::kOk;
    }

    std::optional<std::string> HttpRequest::findHeader_(std::string_view key) const
    {
        auto toLower = [](std::string_view in)
        {
            std::string out;
            out.reserve(in.size());
            for (char c : in)
            {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            return out;
        };

        const auto target = toLower(key);
        for (const auto &kv : headers_)
        {
            if (toLower(kv.first) == target)
            {
                return kv.second;
            }
        }
        return std::nullopt;
    }

    const std::string *HttpRequest::getPathParam(const std::string &key) const
    {
        auto it = pathParams_.find(key);
        if (it != pathParams_.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    void HttpRequest::setPathParam(const std::string &key, std::string value)
    {
        pathParams_[key] = std::move(value);
    }
} // namespace reactor::net::protocol
// namespace reactor::net::protocol
