#include "persistence/ThreadLocalSqlConn.hpp"

#include <chrono>

namespace reactor::persistence
{
    thread_local ThreadLocalSingleConn::Slot ThreadLocalSingleConn::tls_{};

    ThreadLocalSingleConn::ThreadLocalSingleConn(ThreadLocalConnOptions cfg, ConnFactory factory)
        : cfg_(cfg), factory_(factory)
    {}


    void ThreadLocalSingleConn::cleanupCurrentThread()
    {
        auto &s = slot_();

        if (s.conn)
        {
            s.conn->close();
        }

        s.conn.reset();
        s.broken = false;
    }

    ThreadLocalSingleConn::Slot &ThreadLocalSingleConn::slot_()
    {
        return tls_;
    }

    void ThreadLocalSingleConn::ensureConnected_()
    {
        auto &s = slot_();
        const auto now = std::chrono::steady_clock::now();
        const bool isExpiredLifetime =
            s.conn && (now - s.created) > std::chrono::milliseconds(cfg_.maxConnLifetimeMs);
        const bool isExpiredIdle =
            s.conn && (now - s.lastUsed) > std::chrono::milliseconds(cfg_.maxConnIdleMs);

        if (!s.conn || s.broken || isExpiredLifetime || isExpiredIdle)
        {
            if (s.conn)
            {
                s.conn->close();
            }
            s.conn = factory_();
            auto res = s.conn->connect();
            if (!res.ok)
            {
                throw std::runtime_error("sql connect failed: " + res.err.message);
            }
            s.created = now;
            s.lastUsed = now;
            s.broken = false;
        }

        if (cfg_.pingBeforeUse)
        {
            auto pr = s.conn->ping();
            if (!pr.ok)
            {
                s.broken = true;
                throw std::runtime_error("sql ping failed: " + pr.err.message);
            }
        }
    }
} // namespace reactor::persistence
