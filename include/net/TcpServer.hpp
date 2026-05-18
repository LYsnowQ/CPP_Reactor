#pragma once

#include <cstdint>
#include <memory>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <vector>

#include "net/TcpConnection.hpp"
#include "core/EventLoop.hpp"
#include "net/IOThreadPool.hpp"
#include "observability/Metrics.hpp"

namespace reactor::net
{

    // enum class TcpServerMode:uint16_t
    // {
    //     kMianThreadMode,
    //     kChiledThreadMode
    // };

    class TcpServer
    {
      public:
        using RequestHandler = TcpConnection::RequestHandler;

        TcpServer(uint16_t port, uint32_t maxThread,
                  core::DispatcherType dispatcherType = core::DispatcherType::kEpoll,
                  bool keepAliveEnabled = false, uint32_t keepAliveMaxRequests = 100,
                  uint32_t keepAliveIdleTimeoutMs = 10000);
        ~TcpServer();

        core::StatusCode run();

        core::StatusCode acceptConnection();

        core::StatusCode stop();
        void setRequestHandler(RequestHandler handler);

      private:
        void cleanupClosedConnections_();
        void enqueueClosedConnection_(int fd);
        void logMetricsIfNeeded_();

      private:
        int lfd_;
        uint16_t port_;
        std::thread accepter_;
        std::map<int, std::shared_ptr<TcpConnection>> conns_;
        std::mutex connsMutex_;
        std::vector<int> pendingCloseFds_;
        std::mutex pendingCloseMutex_;
        std::unique_ptr<core::EventLoop> baseLoop_;
        std::unique_ptr<IOThreadPool> threadPool_;
        core::DispatcherType dispatcherType_;
        bool keepAliveEnabled_;
        uint32_t keepAliveMaxRequests_;
        uint32_t keepAliveIdleTimeoutMs_;
        RequestHandler requestHandler_;
        std::atomic<bool> isRunning_{false};
        std::chrono::steady_clock::time_point lastMetricsLogTime_;
        reactor::observability::MetricsSnapshot lastMetricsSnapshot_;
    };
} // namespace reactor::net
