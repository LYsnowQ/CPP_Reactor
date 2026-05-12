#pragma once

#include <filesystem>

#include "net/TcpConnection.hpp"

namespace reactor::handler
{
    class StaticFileHandler
    {
      public:
        static net::TcpConnection::RequestHandler createHandler(const std::filesystem::path &root);
    };
} // namespace reactor::handler
