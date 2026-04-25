#include <cstdint>
#include <memory>
#include <system_error>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <chrono>

#include "TcpConnection.hpp"
#include "TcpServer.hpp"    
#include "spdlog/spdlog.h"



reactor::net::TcpServer::TcpServer(
    uint16_t port,
    uint32_t maxThread,
    core::DispatcherType dispatcherType,
    bool keepAliveEnabled,
    uint32_t keepAliveMaxRequests,
    uint32_t keepAliveIdleTimeoutMs)
:lfd_(-1),
 port_(port),
 baseLoop_(nullptr),
 dispatcherType_(dispatcherType),
 keepAliveEnabled_(keepAliveEnabled),
 keepAliveMaxRequests_((keepAliveMaxRequests == 0) ? 1U : keepAliveMaxRequests),
 keepAliveIdleTimeoutMs_((keepAliveIdleTimeoutMs == 0) ? 1U : keepAliveIdleTimeoutMs),
 lastMetricsLogTime_(std::chrono::steady_clock::now())
{
    lfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(lfd_ == -1)
    {
        throw std::system_error(
                    errno,
                    std::system_category(),
                    "create socket failed"
                    );
    }
    
    int opt = 1;
    int ret = setsockopt(lfd_,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(ret == -1)
    {
        throw std::system_error(
                errno,
                std::system_category(),
                "set socket failed"
                );
    }
    
    struct sockaddr_in addr;
    addr.sin_port = htons(port_);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd_, (struct sockaddr*)&addr,sizeof(addr));
    if(ret == -1)
    {
        throw std::system_error(
                errno,
                std::system_category(),
                "bind failed"
                );
    }

    const int flags = fcntl(lfd_, F_GETFL, 0);
    if(flags != -1)
    {
        fcntl(lfd_, F_SETFL, flags | O_NONBLOCK);
    }
    threadPool_ = std::make_unique<reactor::net::IOThreadPool>(maxThread, dispatcherType_);
    lastMetricsSnapshot_ = reactor::observability::Metrics::instance().snapshot();
}

reactor::net::TcpServer::~TcpServer()
{
    stop();
}

void reactor::net::TcpServer::setRequestHandler(RequestHandler handler)
{
    requestHandler_ = std::move(handler);
}


reactor::core::StatusCode reactor::net::TcpServer::run()
{
    if(isRunning_.exchange(true))
    {
        return core::StatusCode::kAgain;
    }
    threadPool_->start();
    int ret = listen(lfd_,128);
    if(ret == -1)
    {
        isRunning_.store(false);
        return core::StatusCode::kError;
    }
    // if(mode == TcpServerMode::kChiledThreadMode)
    // {
        // accepter_ = std::thread(std::bind(&TcpServer::acceptConnection,this));
    // }
    // else if(mode == TcpServerMode::kMianThreadMode)
    // {
    return acceptConnection();
    // }
    // return core::StatusCode::kOk;
}

reactor::core::StatusCode reactor::net::TcpServer::acceptConnection()
{
    while(isRunning_.load())
    {
        cleanupClosedConnections_();
        logMetricsIfNeeded_();
        int cfd = accept(lfd_,nullptr,nullptr);
        if(cfd < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            if(!isRunning_.load())
            {
                break;
            }
            reactor::observability::Metrics::instance().onAcceptFail();
            return core::StatusCode::kError;
        }
        reactor::observability::Metrics::instance().onAcceptOk();

        reactor::core::EventLoop* evloop = threadPool_->getNextLoop();
        if(!evloop)
        {
            close(cfd);
            reactor::observability::Metrics::instance().onAcceptFail();
            continue;
        }

        auto conn = reactor::net::TcpConnection::create(cfd,evloop);
        if(!conn)
        {
            close(cfd);
            reactor::observability::Metrics::instance().onAcceptFail();
            continue;
        }

        reactor::net::TcpConnection* connRaw = nullptr;
        {
            std::lock_guard<std::mutex> lk(connsMutex_);
            auto [it, inserted] = conns_.emplace(cfd,std::move(conn));
            if(!inserted || !it->second)
            {
                close(cfd);
                reactor::observability::Metrics::instance().onAcceptFail();
                continue;
            }
            connRaw = it->second.get();
        }
        connRaw->setCloseCallback(
            [this](int fd)
            {
                enqueueClosedConnection_(fd);
            }
        );
        connRaw->setKeepAliveEnabled(keepAliveEnabled_);
        connRaw->setKeepAlivePolicy(keepAliveMaxRequests_, keepAliveIdleTimeoutMs_);
        if(requestHandler_)
        {
            connRaw->setRequestHandler(requestHandler_);
        }
        if(!connRaw->init())
        {
            std::lock_guard<std::mutex> lk(connsMutex_);
            conns_.erase(cfd);
            close(cfd);
            reactor::observability::Metrics::instance().onAcceptFail();
            continue;
        }
        reactor::observability::Metrics::instance().onConnectionOpened();
    }
    cleanupClosedConnections_();
    logMetricsIfNeeded_();
    return core::StatusCode::kOk;
}

reactor::core::StatusCode reactor::net::TcpServer::stop()
{
    if(!isRunning_.exchange(false))
    {
        return core::StatusCode::kAgain;
    }

    if(lfd_ >= 0)
    {
        close(lfd_);
        lfd_ = -1;
    }

    if(threadPool_)
    {
        threadPool_->stop();
    }

    if(accepter_.joinable())
    {
        accepter_.join();
    }

    {
        std::lock_guard<std::mutex> lk(connsMutex_);
        const auto remaining = conns_.size();
        conns_.clear();
        reactor::observability::Metrics::instance().onConnectionsClosed(static_cast<uint64_t>(remaining));
    }
    logMetricsIfNeeded_();
    return core::StatusCode::kOk;
}

void reactor::net::TcpServer::cleanupClosedConnections_()
{
    if(keepAliveEnabled_)
    {
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
        std::lock_guard<std::mutex> lk(connsMutex_);
        for(auto& [fd, conn] : conns_)
        {
            if(conn && conn->shouldCloseForIdle(nowMs))
            {
                conn->handleClose();
            }
        }
    }

    std::vector<int> localClosedFds;
    {
        std::lock_guard<std::mutex> lk(pendingCloseMutex_);
        if(pendingCloseFds_.empty())
        {
            return;
        }
        std::swap(localClosedFds, pendingCloseFds_);
    }

    std::lock_guard<std::mutex> lk(connsMutex_);
    uint64_t removed = 0;
    for(int fd : localClosedFds)
    {
        auto it = conns_.find(fd);
        if(it != conns_.end())
        {
            conns_.erase(it);
            ++removed;
        }
    }
    reactor::observability::Metrics::instance().onConnectionsClosed(removed);
}

void reactor::net::TcpServer::enqueueClosedConnection_(int fd)
{
    if(fd < 0)
    {
        return;
    }
    std::lock_guard<std::mutex> lk(pendingCloseMutex_);
    pendingCloseFds_.push_back(fd);
}

void reactor::net::TcpServer::logMetricsIfNeeded_()
{
    constexpr auto kLogInterval = std::chrono::seconds(5);
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = now - lastMetricsLogTime_;
    if(elapsed < kLogInterval)
    {
        return;
    }

    const auto cur = reactor::observability::Metrics::instance().snapshot();
    const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    const double elapsedSec = static_cast<double>(elapsedUs) / 1000000.0;

    const auto reqDelta = cur.requestsTotal - lastMetricsSnapshot_.requestsTotal;
    const auto accOkDelta = cur.acceptOk - lastMetricsSnapshot_.acceptOk;
    const auto accFailDelta = cur.acceptFail - lastMetricsSnapshot_.acceptFail;
    const auto readDelta = cur.bytesRead - lastMetricsSnapshot_.bytesRead;
    const auto writeDelta = cur.bytesWritten - lastMetricsSnapshot_.bytesWritten;
    const auto resp2xxDelta = cur.responses2xx - lastMetricsSnapshot_.responses2xx;
    const auto resp4xxDelta = cur.responses4xx - lastMetricsSnapshot_.responses4xx;
    const auto resp5xxDelta = cur.responses5xx - lastMetricsSnapshot_.responses5xx;
    const auto latencySamplesDelta = cur.requestLatencySamples - lastMetricsSnapshot_.requestLatencySamples;
    const auto latencyTotalDelta = cur.requestLatencyTotalUs - lastMetricsSnapshot_.requestLatencyTotalUs;

    const double qps = (elapsedSec > 0.0) ? (static_cast<double>(reqDelta) / elapsedSec) : 0.0;
    const double readBps = (elapsedSec > 0.0) ? (static_cast<double>(readDelta) / elapsedSec) : 0.0;
    const double writeBps = (elapsedSec > 0.0) ? (static_cast<double>(writeDelta) / elapsedSec) : 0.0;
    const double avgLatencyUs = (latencySamplesDelta > 0)
                                    ? (static_cast<double>(latencyTotalDelta) / static_cast<double>(latencySamplesDelta))
                                    : 0.0;

    spdlog::info(
        "[metrics] window={:.2f}s qps={:.2f} active_conn={} accept_ok={} accept_fail={} resp_2xx={} resp_4xx={} "
        "resp_5xx={} read_Bps={:.2f} write_Bps={:.2f} avg_latency_us={:.2f} max_latency_us={}",
        elapsedSec,
        qps,
        cur.activeConnections,
        accOkDelta,
        accFailDelta,
        resp2xxDelta,
        resp4xxDelta,
        resp5xxDelta,
        readBps,
        writeBps,
        avgLatencyUs,
        cur.requestLatencyMaxUs);

    lastMetricsSnapshot_ = cur;
    lastMetricsLogTime_ = now;
}
