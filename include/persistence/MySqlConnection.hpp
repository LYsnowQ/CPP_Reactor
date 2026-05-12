#pragma once

#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlConfig.hpp"
#include "persistence/SqlTypes.hpp"

#include "mysqlx/xdevapi.h"

#include <string_view>
#include <memory>

namespace reactor::persistence
{

    class MySqlConnection final : public ISqlConnection
    {
    public:
        explicit MySqlConnection(const SqlConfig& cfg);
        ~MySqlConnection() override;

        Result<void> connect() override;
        Result<void> ping() override;
        Result<SqlRows> query(std::string_view sql,const std::vector<SqlValue>& args)override;
        Result<uint64_t> execute(std::string_view sql,const std::vector<SqlValue>& args)override;
        void close() noexcept override;

        Result<void> begin() override;
        Result<void> commit() override;
        Result<void> rollback() override;
    private:
        SqlConfig cfg_;
        std::unique_ptr<mysqlx::Session> Session_;
    };

}//namespace reactor::persistence
