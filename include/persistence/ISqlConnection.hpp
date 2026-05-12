#pragma once

#include "persistence/SqlTypes.hpp"

#include <string_view>
#include <vector>

namespace reactor::persistence
{
    class ISqlConnection
    {
      public:
        virtual ~ISqlConnection() = default;
        virtual Result<void> connect() = 0;
        virtual Result<void> ping() = 0;
        virtual Result<SqlRows> query(std::string_view sql, const std::vector<SqlValue> &args) = 0;
        virtual Result<uint64_t> execute(std::string_view sql, const std::vector<SqlValue> &args) = 0;
        virtual void close() noexcept = 0;

        virtual Result<void> begin() = 0;
        virtual Result<void> commit() = 0;
        virtual Result<void> rollback() = 0;
    };

} // namespace reactor::persistence
