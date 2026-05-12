#pragma once
#include "persistence/ISqlConnection.hpp"
#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace reactor::persistence
{

    struct ThreadLocalConnOptions
    {
        bool pingBeforeUse{true};
        uint32_t maxConnLifetimeMs{30 * 60 * 1000};
        uint32_t maxConnIdleMs{60 * 1000};
    };

    class ThreadLocalSingleConn
    {
      public:
        using ConnFactory = std::function<std::unique_ptr<ISqlConnection>()>;

        ThreadLocalSingleConn(ThreadLocalConnOptions cfg, ConnFactory factory);

        template <class F>
        auto withConnection(F &&fn) -> std::invoke_result_t<F, ISqlConnection &>;

        void cleanupCurrentThread();

      private:
        struct Slot
        {
            std::unique_ptr<ISqlConnection> conn;
            std::chrono::steady_clock::time_point created{};
            std::chrono::steady_clock::time_point lastUsed{};
            bool broken{false};
        };

        Slot &slot_();
        void ensureConnected_();

      private:
        static thread_local Slot tls_;
        ThreadLocalConnOptions cfg_;
        ConnFactory factory_;
    };

    template <class F>
    auto ThreadLocalSingleConn::withConnection(F &&fn) -> std::invoke_result_t<F, ISqlConnection &>
    {
        ensureConnected_();
        auto &s = slot_();
        s.lastUsed = std::chrono::steady_clock::now();
        return fn(*s.conn);
    }
} // namespace reactor::persistence
