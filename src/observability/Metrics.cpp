#include "Metrics.hpp"

namespace reactor::observability
{
    Metrics& Metrics::instance()
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

    void Metrics::onConnectionsClosed(uint64_t count)
    {
        if(count == 0)
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
        if(statusCode >= 200 && statusCode < 300)
        {
            responses2xx_.fetch_add(1, std::memory_order_relaxed);
        }
        else if(statusCode >= 400 && statusCode < 500)
        {
            responses4xx_.fetch_add(1, std::memory_order_relaxed);
        }
        else if(statusCode >= 500 && statusCode < 600)
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

    void Metrics::onRequestLatencyUs(uint64_t latencyUs)
    {
        requestLatencySamples_.fetch_add(1, std::memory_order_relaxed);
        requestLatencyTotalUs_.fetch_add(latencyUs, std::memory_order_relaxed);

        uint64_t currentMax = requestLatencyMaxUs_.load(std::memory_order_relaxed);
        while(currentMax < latencyUs && !requestLatencyMaxUs_.compare_exchange_weak(
                                      currentMax,
                                      latencyUs,
                                      std::memory_order_relaxed,
                                      std::memory_order_relaxed))
        {
        }
    }

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
} // 命名空间 reactor::observability
