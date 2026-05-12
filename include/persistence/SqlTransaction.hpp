#pragma once
#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlTypes.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace reactor::persistence
{
    class SqlTransaction
    {
    public:
        explicit SqlTransaction(ISqlConnection& conn);

        ~SqlTransaction();

        SqlTransaction(const SqlTransaction&) = delete;
        SqlTransaction& operator=(const SqlTransaction&) = delete;
        SqlTransaction(SqlTransaction&&) = default;
        SqlTransaction& operator=(SqlTransaction&&) = delete;

        bool valid() const;
        Result<void> commit();
        Result<void> rollback();
        Result<SqlRows> query(std::string_view sql,const std::vector<SqlValue>& args);
        Result<uint64_t> execute(std::string_view sql,const std::vector<SqlValue>& args);
        private:
        ISqlConnection& conn_;
        Result<void> begin_;
        bool finished_{false};
    };


}//namespace reactor::persistence
