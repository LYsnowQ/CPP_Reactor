#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <optional>
#include "core/Buffer.hpp"
#include "core/CoreStatus.hpp"

namespace reactor::net::protocol
{

    class HttpRequest
    {

      public:
        static constexpr size_t kMaxBodyBytes = 1024 * 1024; // 1MB 上限

        struct ParseResult
        {
            core::StatusCode code;
            std::unique_ptr<HttpRequest> request;
            std::string message;
            bool tooLarge = false;
        };

        static ParseResult parseRequest(base::Buffer *data,
                                        std::unique_ptr<HttpRequest> *inFlight = nullptr);

        HttpRequest(const HttpRequest &) = delete;
        HttpRequest &operator=(const HttpRequest &) = delete;

        std::string getMethod();
        std::string getUrl();
        std::string version();

        std::vector<std::pair<std::string, std::string>> getHeader();

        std::string getBody();

        // 路径参数（由 HttpRouter 分派时注入）
        const std::string *getPathParam(const std::string &key) const;
        void setPathParam(const std::string &key, std::string value);

      private:
        enum class HttpRequestState
        {
            kIdle,
            kParseReqLine,
            kParseReqLineFailed,
            kParseReqHeaders,
            kParseReqHeadersFailed,
            kParseReqBody,
            kParseReqBodyFailed,
            kParseDone
        };

        explicit HttpRequest(base::Buffer *data);

        core::StatusCode parse_();

        core::StatusCode parseHead_();
        core::StatusCode parseLine_();
        core::StatusCode parseBody_();
        std::optional<std::string> findHeader_(std::string_view key) const;

      private:
        reactor::base::Buffer *data_;
        std::string method_;
        std::string url_;
        std::string version_;
        std::vector<std::pair<std::string, std::string>> headers_;
        std::string body_;
        bool bodyTooLarge_ = false;

        HttpRequestState curState_;

        std::unordered_map<std::string, std::string> pathParams_;
    };

} // namespace reactor::net::protocol

