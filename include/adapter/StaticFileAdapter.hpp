#pragma once

#include <filesystem>

#include "TcpConnection.hpp"

namespace reactor::net::adapter
{
    class StaticFileAdapter
    {
    public:
        static TcpConnection::RequestHandler createHandler(const std::filesystem::path& root);
    };
}
