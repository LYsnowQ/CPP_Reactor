#include "net/TcpConnection.hpp"
#include "net/Channel.hpp"
#include "core/EventLoop.hpp"
#include "protocol/HttpRequest.hpp"
#include "observability/Metrics.hpp"

#include <cerrno>
#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

#include "spdlog/spdlog.h"

namespace reactor::net
{
    namespace
    {
        int64_t nowSteadyMs_()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                .count();
        }

        std::string toLowerCopy_(std::string_view in)
        {
            std::string out;
            out.reserve(in.size());
            for (char c : in)
            {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            return out;
        }

        // ====================================================================
        // Keep-Alive 判断逻辑
        // ====================================================================
        //
        // HTTP/1.1 默认保持连接，除非 Connection: close。
        // HTTP/1.0 默认关闭连接，除非 Connection: keep-alive。
        // 大小写不敏感比较。
        bool shouldKeepAlive_(protocol::HttpRequest &req)
        {
            const auto versionLower = toLowerCopy_(req.version());
            const auto headers = req.getHeader();

            std::string connectionLower;
            for (const auto &kv : headers)
            {
                if (toLowerCopy_(kv.first) == "connection")
                {
                    connectionLower = toLowerCopy_(kv.second);
                    break;
                }
            }

            if (versionLower == "http/1.0")
            {
                return connectionLower.find("keep-alive") != std::string::npos;
            }
            return connectionLower.find("close") == std::string::npos;
        }
    } // namespace

    // ====================================================================
    // 工厂方法
    // ====================================================================
    //
    // 构造函数为私有，通过 create 静态方法构造 shared_ptr，
    // 使 TcpConnection 支持 shared_from_this，用于异步回写场景。
    std::shared_ptr<TcpConnection> TcpConnection::create(int fd, core::EventLoop *loop)
    {
        auto conn = std::shared_ptr<TcpConnection>(
            new TcpConnection(fd, loop, "Connection-" + std::to_string(fd)));
        return conn;
    }

    // ====================================================================
    // 构造
    // ====================================================================
    //
    // 初始状态为 kConnecting，init() 成功后转为 kConnected。
    // lastActivityMs_ 在构造时记录当前时间，使空闲超时从连接建立开始计算。
    TcpConnection::TcpConnection(int fd, core::EventLoop *loop, std::string name)
        : fd_(fd), state_(kConnecting), loop_(loop), channel_(nullptr), name_(std::move(name)),
          parseWaitStart_(std::chrono::steady_clock::now()),
          requestStartTime_(std::chrono::steady_clock::now()), parseWaiting_(false)
    {
        lastActivityMs_.store(nowSteadyMs_(), std::memory_order_relaxed);
    }

    TcpConnection::~TcpConnection()
    {
        destroy_();
    }

    int TcpConnection::fd() const
    {
        return fd_;
    }

    const std::string &TcpConnection::name() const
    {
        return name_;
    }

    void TcpConnection::destroy_()
    {
        channel_ = nullptr;
        state_ = kDisconnected;
    }

    // ====================================================================
    // Channel 注册
    // ====================================================================
    //
    // 创建 Channel 时绑定自身成员函数（handleRead/handleWrite/closeCallback）。
    // 初始仅监听读事件，写事件在需要发送响应时动态添加。
    bool TcpConnection::init()
    {
        auto ch = std::make_unique<net::Channel>(
            fd_, 
            FDEvent::kReadEvent, 
            std::bind(&TcpConnection::handleRead, this),
            std::bind(&TcpConnection::handleWrite, this),
            std::bind(&TcpConnection::onChannelDestroyed_, this));

        channel_ = ch.get();
        loop_->addTask(std::move(ch), core::ChannelOP::ADD);
        state_ = kConnected;
        return true;
    }

    void TcpConnection::setKeepAliveEnabled(bool enabled)
    {
        keepAliveEnabled_ = enabled;
    }

    void TcpConnection::setKeepAlivePolicy(uint32_t maxRequests, uint32_t idleTimeoutMs)
    {
        maxKeepAliveRequests_ = (maxRequests == 0) ? 1U : maxRequests;
        keepAliveIdleTimeoutMs_ = (idleTimeoutMs == 0) ? 1U : idleTimeoutMs;
    }

    void TcpConnection::setRequestHandler(RequestHandler handler)
    {
        requestHandler_ = std::move(handler);
    }

    void TcpConnection::setCloseCallback(std::function<void(int)> closeCb)
    {
        closeCallback_ = std::move(closeCb);
    }

    bool TcpConnection::shouldCloseForIdle(int64_t nowMs) const
    {
        if (!keepAliveEnabled_)
        {
            return false;
        }
        if (state_.load() != kConnected)
        {
            return false;
        }
        if (keepAliveIdleTimeoutMs_ == 0)
        {
            return false;
        }
        const auto lastMs = lastActivityMs_.load(std::memory_order_relaxed);
        return (nowMs - lastMs) >= static_cast<int64_t>(keepAliveIdleTimeoutMs_);
    }

    bool TcpConnection::isDisconnected() const
    {
        return state_.load() == kDisconnected;
    }

    // ====================================================================
    // Channel 销毁回调
    // ====================================================================
    //
    // 在 Channel 析构时由 EventLoop::remove_ 触发。
    // 清空 channel_ 指针，状态置为 kDisconnected，调用 closeCallback_
    // 通知 TcpServer 从连接容器中移除。
    void TcpConnection::onChannelDestroyed_()
    {
        channel_ = nullptr;
        state_.store(kDisconnected);
        if (closeCallback_)
        {
            closeCallback_(fd_);
        }
    }

    // ====================================================================
    // 读事件处理（HTTP 请求解析主入口）
    // ====================================================================
    //
    // 1. 从 socket 读取数据到 readBuffer_。
    // 2. 增量解析 HTTP 请求（支持一个 TCP 包分多次到达）。
    // 3. 解析未完成（kAgain）：首次等待时记录 parseWaitStart_ 用于超时检测。
    //    若等待超过 5 秒，直接回 408 并断开。
    // 4. 解析失败（400/413）：清空 inFlightRequest_，标记不 Keep-Alive，回错误响应。
    // 5. 解析成功：记录指标，判断是否需要 Keep-Alive，调用 requestHandler_。
    //    若 handler 返回 async=true，跳过同步写路径，等待 sendAsyncResponse。
    //    否则将结果暂存到 pending 变量，启用写事件。
    void TcpConnection::handleRead()
    {
        int savedErr = 0;
        const ssize_t n = readBuffer_.readFd(channel_->getSocket(), &savedErr);

        if (n > 0)
        {
            lastActivityMs_.store(nowSteadyMs_(), std::memory_order_relaxed);
            reactor::observability::Metrics::instance().onBytesRead(static_cast<uint64_t>(n));
            if (!requestTimingActive_)
            {
                requestTimingActive_ = true;
                requestStartTime_ = std::chrono::steady_clock::now();
            }

            auto parseResult = protocol::HttpRequest::parseRequest(&readBuffer_, &inFlightRequest_);

            if (parseResult.code == core::StatusCode::kAgain)
            {
                if (!parseWaiting_)
                {
                    parseWaiting_ = true;
                    parseWaitStart_ = std::chrono::steady_clock::now();
                }
                else if (isParseWaitTimeout_())
                {
                    queueSimpleResponse_(408, "Request Timeout");
                }
                // 请求尚未收全，继续等下一批数据/超时后由写事件回包。
                return;
            }
            parseWaiting_ = false;

            if (parseResult.code != core::StatusCode::kOk || !parseResult.request)
            {
                inFlightRequest_.reset();
                keepAliveRequest_ = false;
                if (parseResult.tooLarge)
                {
                    queueSimpleResponse_(413, "Payload Too Large");
                }
                else
                {
                    queueSimpleResponse_(400, "Bad Request");
                }
                return;
            }
            request_ = std::move(parseResult.request);
            inFlightRequest_.reset();
            reactor::observability::Metrics::instance().onRequestParsed();
            keepAliveRequest_ = keepAliveEnabled_ && shouldKeepAlive_(*request_);
            pendingStatusCode_ = 200;
            pendingReasonPhrase_ = "OK";
            pendingBody_.clear();
            pendingContentType_ = "text/plain; charset=utf-8";

            if (requestHandler_)
            {
                HandlerResult result = requestHandler_(*request_,*this);
                    
                // 异步执行标记，直接返回。
                // 响应结果将通过 sendAsyncResponse 在后台线程回写。
                if(result.async)
                {
                    asyncPending_=true;
                    return;
                }
                
                // 同步机制：将 handler 返回的结果暂存到 pending 变量。
                if (result.statusCode >= 100 && result.statusCode <= 599)
                {
                    pendingStatusCode_ = result.statusCode;
                }
                if (!result.reasonPhrase.empty())
                {
                    pendingReasonPhrase_ = std::move(result.reasonPhrase);
                }
                pendingBody_ = std::move(result.body);
                if (!result.contentType.empty())
                {
                    pendingContentType_ = std::move(result.contentType);
                }
                if (result.closeConnection)
                {
                    keepAliveRequest_ = false;
                }
            }

            if (channel_)
            {
                channel_->writeEventEnable(true);
            }
            loop_->addTask(channel_->getSocket(), core::ChannelOP::MODIFY);
        }
        else if (n == 0)
        {
            handleClose();
        }
        else if (savedErr != EAGAIN && savedErr != EWOULDBLOCK)
        {
            handleClose();
        }
    }

    bool TcpConnection::isParseWaitTimeout_() const
    {
        constexpr auto kParseWaitTimeout = std::chrono::seconds(5);
        return std::chrono::steady_clock::now() - parseWaitStart_ >= kParseWaitTimeout;
    }

    // ====================================================================
    // HTTP 响应组包
    // ====================================================================
    //
    // 同步组装 HTTP/1.1 响应，包含：
    //   状态行：HTTP/1.1 <statusCode> <reasonPhrase>
    //   响应头：Content-Type、Content-Length、Connection、Server
    //   空行 + 响应体
    // 写入 writeBuffer_，由后续 handleWrite 统一发送。
    //
    // 同时对本次请求记录响应状态码和时延指标。
    void TcpConnection::appendSimpleResponse_(int statusCode, std::string_view reasonPhrase,
                                              std::string_view body, std::string_view contentType)
    {
        reactor::observability::Metrics::instance().onResponseStatus(statusCode);
        if (requestTimingActive_)
        {
            const auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now() - requestStartTime_)
                                     .count();
            reactor::observability::Metrics::instance().onRequestLatencyUs(
                static_cast<uint64_t>(latency));
            requestTimingActive_ = false;
        }

        std::string line = "HTTP/1.1 " + std::to_string(statusCode) + " " +
                           std::string(reasonPhrase) +
                           "\r\n"
                           "Content-Type: " +
                           std::string(contentType) +
                           "\r\n"
                           "Content-Length: " +
                           std::to_string(body.size()) +
                           "\r\n"
                           "Connection: " +
                           std::string(keepAliveRequest_ ? "keep-alive" : "close") +
                           "\r\n"
                           "Server: CPPReactor\r\n"
                           "\r\n";
        writeBuffer_.append(line);
        if (!body.empty())
        {
            writeBuffer_.append(body.data(), body.size());
        }
    }

    void TcpConnection::queueSimpleResponse_(int statusCode, std::string_view reasonPhrase)
    {
        appendSimpleResponse_(statusCode, reasonPhrase, "", "text/plain; charset=utf-8");
        if (channel_)
        {
            channel_->writeEventEnable(true);
        }
        loop_->addTask(channel_->getSocket(), core::ChannelOP::MODIFY);
    }

    // ====================================================================
    // 写事件处理（HTTP 响应发送主入口）
    // ====================================================================
    //
    // 1. 若 writeBuffer_ 为空（首次写入），调用 appendSimpleResponse_
    //    组装 HTTP 响应行 + 头 + 体到 writeBuffer_。
    // 2. writeFd 将 buffer 内容写入 socket。
    // 3. 发送完毕后：
    //      - Keep-Alive 模式：重置 pending 变量 + 禁用写事件 + 等待下一请求。
    //        servedRequests_ 递增，达到上限时关闭。
    //      - 非 Keep-Alive：handleClose。
    // 4. writeFd 返回错误（非 EAGAIN）时直接关闭连接。
    void TcpConnection::handleWrite()
    {
        if (writeBuffer_.readableBytes() == 0)
        {
            appendSimpleResponse_(pendingStatusCode_, pendingReasonPhrase_, pendingBody_,
                                  pendingContentType_);
        }

        const ssize_t n = writeBuffer_.writeFd(fd_);
        if (n > 0)
        {
            lastActivityMs_.store(nowSteadyMs_(), std::memory_order_relaxed);
            reactor::observability::Metrics::instance().onBytesWritten(static_cast<uint64_t>(n));
        }
        if (n < 0 && errno != EAGAIN)
        {
            handleClose();
            return;
        }
        if (writeBuffer_.readableBytes() == 0)
        {
            if (keepAliveRequest_)
            {
                keepAliveRequest_ = false;
                const auto served = servedRequests_.fetch_add(1, std::memory_order_relaxed) + 1;
                request_.reset();
                pendingStatusCode_ = 200;
                pendingReasonPhrase_ = "OK";
                pendingBody_.clear();
                pendingContentType_ = "text/plain; charset=utf-8";
                if (served >= static_cast<uint64_t>(maxKeepAliveRequests_))
                {
                    handleClose();
                    return;
                }
                if (channel_)
                {
                    channel_->writeEventEnable(false);
                    loop_->addTask(channel_->getSocket(), core::ChannelOP::MODIFY);
                }
                return;
            }
            handleClose();
        }
    }

    // ====================================================================
    // 连接关闭
    // ====================================================================
    //
    // 幂等设计：先检查当前状态，已关闭或关闭中时直接返回。
    // 投递 DELETE 任务到 EventLoop 移除 Channel，由 onChannelDestroyed_ 完成最终清理。
    void TcpConnection::handleClose()
    {
        // 幂等关闭：防止 handleClose 被重复调用时重复投递 DELETE。
        const auto cur = state_.load();
        if (cur == kDisconnected || cur == kDisconnecting)
        {
            return;
        }
        state_.store(kDisconnecting);
        if (channel_)
        {
            loop_->addTask(channel_->getSocket(), core::ChannelOP::DELETE);
        }
        else
        {
            state_.store(kDisconnected);
        }
    }

    core::EventLoop* TcpConnection::getLoop() const
    {
        return loop_;
    }

    // ====================================================================
    // 异步响应回写
    // ====================================================================
    //
    // 由异步处理器（如 SqlHandler）跨线程调用。
    // 先检查 state_ 确保连接仍存活（已断开则不操作）。
    // 更新 pending 变量 → 启用写事件 → 通知 EventLoop 触发 handleWrite。
    void TcpConnection::sendAsyncResponse(const HandlerResult &result)
    {
        // 连接已断开（如客户端提前关闭），放弃回写。
        if(state_.load() != kConnected)
        {
            return;
        }

        if(result.statusCode >=100 && result.statusCode<=599)
        {
            pendingStatusCode_ = result.statusCode;
        }

        if(!result.reasonPhrase.empty())
        {
            pendingReasonPhrase_ = std::move(result.reasonPhrase);
        }
        
        pendingBody_ = std::move(result.body);
        
        if(!result.contentType.empty())
        {
            pendingContentType_ = std::move(result.contentType);
        }

        asyncPending_ = false;

        if(channel_)
        {
            channel_->writeEventEnable(true);
            loop_->addTask(channel_->getSocket(),core::ChannelOP::MODIFY);
        }
    }

}
