#pragma once

#include <atomic>
#include <cstdint>

namespace reactor::observability
{
    struct MetricsSnapshot
    {
        uint64_t acceptOk = 0;
        uint64_t acceptFail = 0;
        uint64_t activeConnections = 0;
        uint64_t requestsTotal = 0;
        uint64_t responses2xx = 0;
        uint64_t responses4xx = 0;
        uint64_t responses5xx = 0;
        uint64_t bytesRead = 0;
        uint64_t bytesWritten = 0;
        uint64_t requestLatencySamples = 0;
        uint64_t requestLatencyTotalUs = 0;
        uint64_t requestLatencyMaxUs = 0;
    };

    class Metrics
    {
    public:
        static Metrics& instance();

        void onAcceptOk();
        void onAcceptFail();
        void onConnectionOpened();
        void onConnectionsClosed(uint64_t count);
        void onRequestParsed();
        void onResponseStatus(int statusCode);
        void onBytesRead(uint64_t count);
        void onBytesWritten(uint64_t count);
        void onRequestLatencyUs(uint64_t latencyUs);

        MetricsSnapshot snapshot() const;

    private:
        Metrics() = default;

    private:
        std::atomic<uint64_t> acceptOk_{0};
        std::atomic<uint64_t> acceptFail_{0};
        std::atomic<uint64_t> activeConnections_{0};
        std::atomic<uint64_t> requestsTotal_{0};
        std::atomic<uint64_t> responses2xx_{0};
        std::atomic<uint64_t> responses4xx_{0};
        std::atomic<uint64_t> responses5xx_{0};
        std::atomic<uint64_t> bytesRead_{0};
        std::atomic<uint64_t> bytesWritten_{0};
        std::atomic<uint64_t> requestLatencySamples_{0};
        std::atomic<uint64_t> requestLatencyTotalUs_{0};
        std::atomic<uint64_t> requestLatencyMaxUs_{0};
    };
} // 命名空间 reactor::observability
