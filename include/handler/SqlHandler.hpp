#pragma once

#include "net/TcpConnection.hpp"
#include "persistence/SqlExecutor.hpp"
#include <memory>

namespace reactor::handler
{
    class SqlHandler
    {
    public:
      static net::TcpConnection::RequestHandler createHandler(std::shared_ptr<persistence::SqlExecutor> sqlExecutor);  
    };

}//namespace reactor::handler
