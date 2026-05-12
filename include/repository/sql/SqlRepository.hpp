#pragma once
#include "persistence/SqlExecutor.hpp"
#include "persistence/SqlTypes.hpp"
#include <functional>
#include <string>
#include <vector>

namespace reactor::repository::sql
{
    class SqlRepository
    {
    public:
        using Done =  std::function<void(persistence::Result<persistence::SqlRows>)>;

        explicit SqlRepository(persistence::SqlExecutor& executor);

        bool findUserByIdAsync(int64_t userId,Done done);
    private:
        persistence::SqlExecutor& executor_;
    };
}
