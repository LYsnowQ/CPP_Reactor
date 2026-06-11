#include "persistence/ThreadLocalSqlConn.hpp"

#include <chrono>

namespace reactor::persistence
{
    thread_local ThreadLocalSingleConn::Slot ThreadLocalSingleConn::tls_{};

    // ====================================================================
    // 构造
    // ====================================================================
    //
    // 仅保存配置和工厂函数，不创建任何连接。
    // thread_local Slot 在首次使用 slot_() 时默认构造（空连接）。
    ThreadLocalSingleConn::ThreadLocalSingleConn(ThreadLocalConnOptions cfg, ConnFactory factory)
        : cfg_(cfg), factory_(factory)
    {}

    // ====================================================================
    // 线程退出清理
    // ====================================================================
    //
    // 关闭当前线程的数据库连接并释放资源。
    // 重置 broken 标记，使下次使用回到"新建连接"路径。
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

    // ====================================================================
    // 连接保障
    // ====================================================================
    //
    // 以下任意条件成立时重建连接：
    //   1. s.conn 为空（首次使用）
    //   2. s.broken 为 true（上次 ping 失败标记）
    //   3. 超过 maxConnLifetimeMs（连接已达最大生命周期）
    //   4. 超过 maxConnIdleMs（空闲超时）
    //
    // 重建步骤：
    //   1. 若旧连接存在，先 close()
    //   2. factory_() 创建新连接实例
    //   3. connect() 建立网络连接
    //   4. 连接失败时抛出 runtime_error（由调用方捕获）
    //
    // 若 pingBeforeUse 为 true，每次使用前额外执行 ping 验证：
    //   ping 失败 → s.broken = true → 下次 ensureConnected_ 重建
    void ThreadLocalSingleConn::ensureConnected_()
    {
        auto &s = slot_();
        const auto now = std::chrono::steady_clock::now();
        const bool isExpiredLifetime =
            s.conn && (now - s.created) > std::chrono::milliseconds(cfg_.maxConnLifetimeMs);
        const bool isExpiredIdle =
            s.conn && (now - s.lastUsed) > std::chrono::milliseconds(cfg_.maxConnIdleMs);

        // 连接已过期（生命周期/空闲超时/断开），重建新连接。
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

        // ping 失败标记连接为 broken，下次使用时重建。
        // 避免在每次使用前都重建连接的性能损失。
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
