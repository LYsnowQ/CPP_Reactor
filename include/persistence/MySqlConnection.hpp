#pragma once

#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlConfig.hpp"
#include "persistence/SqlTypes.hpp"

#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>

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
        sql::Driver* driver_{nullptr};
        std::unique_ptr<sql::Connection> conn_;
    };

}//namespace reactor::persistence
