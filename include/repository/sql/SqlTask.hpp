#pragma once
#include "persistence/SqlTypes.hpp"
#include "persistence/ISqlConnection.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <chrono>

namespace reactor::repository::sql
{
    struct SqlTaskResult
    {
        persistence::Result<persistence::SqlRows> rows;
        int64_t elapsedMs{0};
    };

    class ILoopPoster
    {
    public:
        virtual ~ILoopPoster() = default;
        virtual void post(std::function<void()> fn) =0;
    };

    struct SqlTask
    {
        uint64_t id{0};
        std::string traceId;
        std::chrono::steady_clock::time_point deadline{};
        std::function<persistence::Result<persistence::SqlRows>(persistence::ISqlConnection&)> run;
        std::function<void(SqlTaskResult)> onDone;
        std::shared_ptr<ILoopPoster> loopPoster;
    };


}//namespace reactor::repository::sql
