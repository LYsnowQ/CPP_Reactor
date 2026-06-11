#include "observability/Metrics.hpp"

namespace reactor::observability
{
    // ====================================================================
    // 单例实现
    // ====================================================================
    //
    // C++11 起 static local 初始化线程安全，无需额外加锁。
    // 在首次调用 instance() 时构造，程序结束时自动析构。
    Metrics &Metrics::instance()
    {
        static Metrics metrics;
        return metrics;
    }

    void Metrics::onAcceptOk()
    {
        acceptOk_.fetch_add(1, std::memory_order_relaxed);
    }

    void Metrics::onAcceptFail()
    {
        acceptFail_.fetch_add(1, std::memory_order_relaxed);
    }

    void Metrics::onConnectionOpened()
    {
        activeConnections_.fetch_add(1, std::memory_order_relaxed);
    }

    // ====================================================================
    // 活跃连接数递减
    // ====================================================================
    //
    // count == 0 时直接返回，避免不必要的原子操作。
    // fetch_sub 允许一次性批量关闭多连接（如 TcpServer::stop 清空容器时）。
    void Metrics::onConnectionsClosed(uint64_t count)
    {
        if (count == 0)
        {
            return;
        }
        activeConnections_.fetch_sub(count, std::memory_order_relaxed);
    }

    void Metrics::onRequestParsed()
    {
        requestsTotal_.fetch_add(1, std::memory_order_relaxed);
    }

    void Metrics::onResponseStatus(int statusCode)
    {
        if (statusCode >= 200 && statusCode < 300)
        {
            responses2xx_.fetch_add(1, std::memory_order_relaxed);
        }
        else if (statusCode >= 400 && statusCode < 500)
        {
            responses4xx_.fetch_add(1, std::memory_order_relaxed);
        }
        else if (statusCode >= 500 && statusCode < 600)
        {
            responses5xx_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void Metrics::onBytesRead(uint64_t count)
    {
        bytesRead_.fetch_add(count, std::memory_order_relaxed);
    }

    void Metrics::onBytesWritten(uint64_t count)
    {
        bytesWritten_.fetch_add(count, std::memory_order_relaxed);
    }

    // ====================================================================
    // 延迟指标记录
    // ====================================================================
    //
    // 采样数 +1，总延迟累加，最大延迟通过 CAS 循环更新。
    // CAS 重试仅在多线程同时写入且当前值已被更新时发生，
    // 大部分情况下一次成功。
    void Metrics::onRequestLatencyUs(uint64_t latencyUs)
    {
        requestLatencySamples_.fetch_add(1, std::memory_order_relaxed);
        requestLatencyTotalUs_.fetch_add(latencyUs, std::memory_order_relaxed);

        // CAS 循环更新最大延迟：
        // 当 latencyUs > currentMax 时尝试交换，若失败则 reload currentMax 重试。
        // 确保高并发下最大延迟值不会丢失。
        uint64_t currentMax = requestLatencyMaxUs_.load(std::memory_order_relaxed);
        while (currentMax < latencyUs &&
               !requestLatencyMaxUs_.compare_exchange_weak(
                   currentMax, latencyUs, std::memory_order_relaxed, std::memory_order_relaxed))
        {
        }
    }

    // ====================================================================
    // 快照采集
    // ====================================================================
    //
    // 读取 12 个原子计数器，使用 memory_order_relaxed。
    // 快照不保证跨字段的一致性——不同计数器可能在 snapshot() 执行期间
    // 被其他线程修改，但对于窗口差值计算来说，这种偏差可以忽略。
    MetricsSnapshot Metrics::snapshot() const
    {
        MetricsSnapshot s;
        s.acceptOk = acceptOk_.load(std::memory_order_relaxed);
        s.acceptFail = acceptFail_.load(std::memory_order_relaxed);
        s.activeConnections = activeConnections_.load(std::memory_order_relaxed);
        s.requestsTotal = requestsTotal_.load(std::memory_order_relaxed);
        s.responses2xx = responses2xx_.load(std::memory_order_relaxed);
        s.responses4xx = responses4xx_.load(std::memory_order_relaxed);
        s.responses5xx = responses5xx_.load(std::memory_order_relaxed);
        s.bytesRead = bytesRead_.load(std::memory_order_relaxed);
        s.bytesWritten = bytesWritten_.load(std::memory_order_relaxed);
        s.requestLatencySamples = requestLatencySamples_.load(std::memory_order_relaxed);
        s.requestLatencyTotalUs = requestLatencyTotalUs_.load(std::memory_order_relaxed);
        s.requestLatencyMaxUs = requestLatencyMaxUs_.load(std::memory_order_relaxed);
        return s;
    }
} // namespace reactor::observability
