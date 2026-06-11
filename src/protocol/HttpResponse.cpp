#include "protocol/HttpResponse.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <system_error>
#include <stdexcept>

namespace reactor::net::protocol
{
    HttpResponse::HttpResponse(base::Buffer *buffer) : ready_(15), response_(buffer)
    {
        checkReady_();
    }

    void HttpResponse::setStateLine(const std::string &version, uint32_t status,
                                    const std::string &statusMsg)
    {
        if (version == "")
        {
            line_ = "HTTP/1.1 " + std::to_string(status) + " " + statusMsg + "\r\n";
        }
        else
        {
            line_ = version + " " + std::to_string(status) + " " + statusMsg + "\r\n";
        }
    }

    void HttpResponse::addHeader(std::string key, std::string value)
    {
        if (key == "" || value == "")
        {
            throw std::invalid_argument("header key or value is empty");
        }
        headers_.emplace_back(std::move(key), std::move(value));
    }

    void HttpResponse::addfile(std::string file)
    { // 只传文件名，后续发送时再打开处理
        files_.emplace_back(std::move(file));
    }

    uint16_t HttpResponse::getCheckReady()
    {
        checkReady_();
        if (ready_ == static_cast<uint16_t>(ReadyCode::Ready))
        {
            std::cout << "response complated\n";
            return 0;
        }

        if (ready_ & static_cast<uint16_t>(ReadyCode::NoResponseLine))
        {
            std::cout << "ResponseLine dosen't exist\n";
        }

        if (ready_ & static_cast<uint16_t>(ReadyCode::NoHeaders))
        {
            std::cout << "ResponseHeaders dosen't exist\n";
        }

        if (ready_ & static_cast<uint16_t>(ReadyCode::NoFile))
        {
            std::cout << "Responsefile dosen't exist\n";
        }

        if (ready_ & static_cast<uint16_t>(ReadyCode::NoResponseBuffer))
        {
            std::cout << "ResponseBuffer dosen't exist\n";
        }
        return ready_;
    }

    void HttpResponse::checkReady_()
    {
        if (!response_)
        {
            ready_ |= static_cast<uint16_t>(ReadyCode::NoResponseBuffer);
        }
        else
        {
            ready_ &= ~static_cast<uint16_t>(ReadyCode::NoResponseBuffer);
        }

        if (line_ == std::string())
        {
            ready_ |= static_cast<uint16_t>(ReadyCode::NoResponseLine);
        }
        else
        {
            ready_ &= ~static_cast<uint16_t>(ReadyCode::NoResponseLine);
        }

        if (headers_.empty())
        {
            ready_ |= static_cast<uint16_t>(ReadyCode::NoHeaders);
        }
        else
        {
            ready_ &= ~static_cast<uint16_t>(ReadyCode::NoHeaders);
        }

        if (files_.empty())
        {
            ready_ |= static_cast<uint16_t>(ReadyCode::NoFile);
        }
        else
        {
            ready_ &= ~static_cast<uint16_t>(ReadyCode::NoFile);
        }
    }
} // namespace reactor::net::protocol
// namespace reactor::net::protocol
