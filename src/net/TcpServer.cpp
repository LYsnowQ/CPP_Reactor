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

#include "net/TcpConnection.hpp"
#include "net/TcpServer.hpp"
#include "spdlog/spdlog.h"

namespace reactor::net
{
    // ====================================================================
    // 构造
    // ====================================================================
    //
    // socket → SO_REUSEADDR → bind → 非阻塞 → IOThreadPool 创建。
    // SO_REUSEADDR 确保端口在 TIME_WAIT 状态下仍可重新绑定。
    // 监听 socket 设为非阻塞，使 accept 返回 EAGAIN/EWOULDBLOCK
    // 时可以短暂休眠而非忙等。
    //
    // Keep-Alive 参数保护：外部传 0 时内部调整为 1，防止除零或立即超时。
    TcpServer::TcpServer(uint16_t port, uint32_t maxThread, core::DispatcherType dispatcherType,
                         bool keepAliveEnabled, uint32_t keepAliveMaxRequests,
                         uint32_t keepAliveIdleTimeoutMs)
        : lfd_(-1), port_(port), dispatcherType_(dispatcherType),
          keepAliveEnabled_(keepAliveEnabled),
          keepAliveMaxRequests_((keepAliveMaxRequests == 0) ? 1U : keepAliveMaxRequests),
          keepAliveIdleTimeoutMs_((keepAliveIdleTimeoutMs == 0) ? 1U : keepAliveIdleTimeoutMs),
          lastMetricsLogTime_(std::chrono::steady_clock::now())
    {
        lfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (lfd_ == -1)
        {
            throw std::system_error(errno, std::system_category(), "create socket failed");
        }

        int opt = 1;
        int ret = setsockopt(lfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (ret == -1)
        {
            throw std::system_error(errno, std::system_category(), "set socket failed");
        }

        struct sockaddr_in addr;
        addr.sin_port = htons(port_);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        ret = bind(lfd_, (struct sockaddr *)&addr, sizeof(addr));
        if (ret == -1)
        {
            throw std::system_error(errno, std::system_category(), "bind failed");
        }

        const int flags = fcntl(lfd_, F_GETFL, 0);
        if (flags != -1)
        {
            fcntl(lfd_, F_SETFL, flags | O_NONBLOCK);
        }
        threadPool_ = std::make_unique<IOThreadPool>(maxThread, dispatcherType_);
        lastMetricsSnapshot_ = reactor::observability::Metrics::instance().snapshot();
    }

    TcpServer::~TcpServer()
    {
        stop();
    }

    void TcpServer::setRequestHandler(RequestHandler handler)
    {
        requestHandler_ = std::move(handler);
    }

    // ====================================================================
    // 启动
    // ====================================================================
    //
    // isRunning_ 的 atomic exchange 确保重复调用安全。
    // 必须先 start IOThreadPool 再 listen，防止 accept 后获取空 EventLoop。
    core::StatusCode TcpServer::run()
    {
        if (isRunning_.exchange(true))
        {
            return core::StatusCode::kAgain;
        }
        threadPool_->start();
        int ret = listen(lfd_, 128);
        if (ret == -1)
        {
            isRunning_.store(false);
            return core::StatusCode::kError;
        }
        return acceptConnection();
    }

    // ====================================================================
    // accept 循环（主线程阻塞）
    // ====================================================================
    //
    // 每轮循环：
    //   1. cleanupClosedConnections_：处理待关闭连接列表 + Keep-Alive 空闲超时检查
    //   2. logMetricsIfNeeded_：每 5 秒输出一次指标
    //   3. accept 新连接：
    //      a. EINTR → 重试
    //      b. EAGAIN/EWOULDBLOCK → sleep(1ms) 后重试
    //      c. 非运行中 → break 退出
    //      d. 其他错误 → 返回 kError
    //
    // 新连接处理：
    //   1. 选择目标 EventLoop（round-robin）
    //   2. TcpConnection::create → init
    //   3. 设置 Keep-Alive 策略和 RequestHandler
    //   4. 存入 conns_ 容器
    //   5. 若 init 失败，回滚（擦除 + close fd）
    //
    // 所有失败路径均调用 Metrics::onAcceptFail 保证计数一致。
    core::StatusCode TcpServer::acceptConnection()
    {
        while (isRunning_.load())
        {
            cleanupClosedConnections_();
            logMetricsIfNeeded_();
            int cfd = accept(lfd_, nullptr, nullptr);
            if (cfd < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 非阻塞 accept 返回 EAGAIN 时短暂休眠，避免 CPU 空转。
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                if (!isRunning_.load())
                {
                    break;
                }
                reactor::observability::Metrics::instance().onAcceptFail();
                return core::StatusCode::kError;
            }
            reactor::observability::Metrics::instance().onAcceptOk();

            core::EventLoop *evloop = threadPool_->getNextLoop();
            if (!evloop)
            {
                close(cfd);
                reactor::observability::Metrics::instance().onAcceptFail();
                continue;
            }

            auto conn = TcpConnection::create(cfd, evloop);
            if (!conn)
            {
                close(cfd);
                reactor::observability::Metrics::instance().onAcceptFail();
                continue;
            }

            TcpConnection *connRaw = nullptr;
            {
                std::lock_guard<std::mutex> lk(connsMutex_);
                auto [it, inserted] = conns_.emplace(cfd, std::move(conn));
                if (!inserted || !it->second)
                {
                    close(cfd);
                    reactor::observability::Metrics::instance().onAcceptFail();
                    continue;
                }
                // 先插入 conns_ 再 setCallback，因为 setCloseCallback
                // 可能在 init 中就被触发。
                connRaw = it->second.get();
            }
            connRaw->setCloseCallback([this](int fd) { enqueueClosedConnection_(fd); });
            connRaw->setKeepAliveEnabled(keepAliveEnabled_);
            connRaw->setKeepAlivePolicy(keepAliveMaxRequests_, keepAliveIdleTimeoutMs_);
            if (requestHandler_)
            {
                connRaw->setRequestHandler(requestHandler_);
            }
            if (!connRaw->init())
            {
                // init 失败时回滚 conns_ 的插入，保证连接容器状态一致。
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

    // ====================================================================
    // 停止
    // ====================================================================
    //
    // 先 close(lfd_) 使 accept 返回错误退出循环，
    // 再 stop IOThreadPool，最后清理连接容器。
    core::StatusCode TcpServer::stop()
    {
        if (!isRunning_.exchange(false))
        {
            return core::StatusCode::kAgain;
        }

        // 先 close 监听 fd 使 accept 退出循环，再停线程池。
        if (lfd_ >= 0)
        {
            close(lfd_);
            lfd_ = -1;
        }

        if (threadPool_)
        {
            threadPool_->stop();
        }

        {
            std::lock_guard<std::mutex> lk(connsMutex_);
            const auto remaining = conns_.size();
            conns_.clear();
            reactor::observability::Metrics::instance().onConnectionsClosed(
                static_cast<uint64_t>(remaining));
        }
        logMetricsIfNeeded_();
        return core::StatusCode::kOk;
    }

    // ====================================================================
    // 连接清理
    // ====================================================================
    //
    // 两个阶段：
    //   1. Keep-Alive 空闲超时检查：遍历 conns_，对超时连接调用 handleClose
    //   2. 待关闭连接收集：从 pendingCloseFds_ 取回已关闭的 fd 列表
    //
    // pendingCloseFds_ 可能被多个 EventLoop 线程同时入队，
    // 所以需要 pendingCloseMutex_ 保护。
    // conns_ 在主线程操作，由 connsMutex_ 保护。
    void TcpServer::cleanupClosedConnections_()
    {
        if (keepAliveEnabled_)
        {
            const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count();
            std::lock_guard<std::mutex> lk(connsMutex_);
            for (auto &[fd, conn] : conns_)
            {
                if (conn && conn->shouldCloseForIdle(nowMs))
                {
                    conn->handleClose();
                }
            }
        }

        // 用本地队列交换的方式处理关闭列表，缩短锁持有时间。
        std::vector<int> localClosedFds;
        {
            std::lock_guard<std::mutex> lk(pendingCloseMutex_);
            if (pendingCloseFds_.empty())
            {
                return;
            }
            std::swap(localClosedFds, pendingCloseFds_);
        }

        std::lock_guard<std::mutex> lk(connsMutex_);
        uint64_t removed = 0;
        for (int fd : localClosedFds)
        {
            auto it = conns_.find(fd);
            if (it != conns_.end())
            {
                conns_.erase(it);
                ++removed;
            }
        }
        reactor::observability::Metrics::instance().onConnectionsClosed(removed);
    }

    // pendingCloseFds_ 可能被多个 EventLoop 线程同时入队，
    // 所以需要 pendingCloseMutex_ 保护。
    void TcpServer::enqueueClosedConnection_(int fd)
    {
        if (fd < 0)
        {
            return;
        }
        std::lock_guard<std::mutex> lk(pendingCloseMutex_);
        pendingCloseFds_.push_back(fd);
    }

    // ====================================================================
    // 指标日志（每 5 秒输出）
    // ====================================================================
    //
    // 计算当前窗口与上一窗口的差值：QPS、吞吐量、平均/最大延迟。
    // 使用 Metrics::snapshot() 获取原子计数器快照，避免每 5 秒日志
    // 对计数器产生额外竞争。
    void TcpServer::logMetricsIfNeeded_()
    {
        constexpr auto kLogInterval = std::chrono::seconds(5);
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = now - lastMetricsLogTime_;
        if (elapsed < kLogInterval)
        {
            return;
        }

        const auto cur = reactor::observability::Metrics::instance().snapshot();
        const auto elapsedUs =
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        const double elapsedSec = static_cast<double>(elapsedUs) / 1000000.0;

        // 计数器差值计算，避免日志行中累积值的阅读歧义。
        const auto reqDelta = cur.requestsTotal - lastMetricsSnapshot_.requestsTotal;
        const auto accOkDelta = cur.acceptOk - lastMetricsSnapshot_.acceptOk;
        const auto accFailDelta = cur.acceptFail - lastMetricsSnapshot_.acceptFail;
        const auto readDelta = cur.bytesRead - lastMetricsSnapshot_.bytesRead;
        const auto writeDelta = cur.bytesWritten - lastMetricsSnapshot_.bytesWritten;
        const auto resp2xxDelta = cur.responses2xx - lastMetricsSnapshot_.responses2xx;
        const auto resp4xxDelta = cur.responses4xx - lastMetricsSnapshot_.responses4xx;
        const auto resp5xxDelta = cur.responses5xx - lastMetricsSnapshot_.responses5xx;
        const auto latencySamplesDelta =
            cur.requestLatencySamples - lastMetricsSnapshot_.requestLatencySamples;
        const auto latencyTotalDelta =
            cur.requestLatencyTotalUs - lastMetricsSnapshot_.requestLatencyTotalUs;

        const double qps = (elapsedSec > 0.0) ? (static_cast<double>(reqDelta) / elapsedSec) : 0.0;
        const double readBps =
            (elapsedSec > 0.0) ? (static_cast<double>(readDelta) / elapsedSec) : 0.0;
        const double writeBps =
            (elapsedSec > 0.0) ? (static_cast<double>(writeDelta) / elapsedSec) : 0.0;
        const double avgLatencyUs = (latencySamplesDelta > 0)
                                        ? (static_cast<double>(latencyTotalDelta) /
                                           static_cast<double>(latencySamplesDelta))
                                        : 0.0;

        spdlog::info(
            "[metrics] window={:.2f}s qps={:.2f} active_conn={} accept_ok={} accept_fail={} "
            "resp_2xx={} resp_4xx={} "
            "resp_5xx={} read_Bps={:.2f} write_Bps={:.2f} avg_latency_us={:.2f} max_latency_us={}",
            elapsedSec, qps, cur.activeConnections, accOkDelta, accFailDelta, resp2xxDelta,
            resp4xxDelta, resp5xxDelta, readBps, writeBps, avgLatencyUs, cur.requestLatencyMaxUs);

        lastMetricsSnapshot_ = cur;
        lastMetricsLogTime_ = now;
    }
} // namespace reactor::net
// namespace reactor::net
