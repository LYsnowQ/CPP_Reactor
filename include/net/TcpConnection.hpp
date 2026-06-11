#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <functional>

#include "net/Channel.hpp"
#include "core/EventLoop.hpp"
#include "core/Buffer.hpp"
#include "protocol/HttpRequest.hpp"
#include "protocol/HttpResponse.hpp"

namespace reactor::net
{

/// @brief TCP 连接封装，管理单条连接上的 HTTP 请求处理生命周期
///
/// 职责：
///   1. 通过 Channel 注册到 EventLoop，接收可读/可写事件
///   2. 读取数据 → 解析 HTTP 请求 → 调用 RequestHandler 处理
///   3. 同步或异步写回响应
///   4. 管理 Keep-Alive 策略（启用/禁用、最大请求数、空闲超时）
///   5. 连接关闭时通过 closeCallback 通知 TcpServer
///
/// 生命周期由 TcpServer 通过 shared_ptr 管理，启用 shared_from_this
/// 以支持异步回调中安全引用（SqlHandler 场景）。
///
/// @thread handleRead/handleWrite/handleClose 仅由所属 EventLoop 线程调用
/// @warning sendAsyncResponse 可跨线程调用（EventLoop::post 投递后执行）
class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
  public:
    /// @brief 请求处理器的返回结果
    ///
    /// 支持同步和异步两种模式：
    ///   同步：直接填写 body/statusCode，TcpConnection 立即组织响应并发送
    ///   异步：设置 async=true，处理器通过 conn.sendAsyncResponse 在稍后回写
    ///
    /// @param statusCode       HTTP 状态码（100-599）
    /// @param reasonPhrase     HTTP 状态描述（如 "OK"、"Not Found"）
    /// @param body             响应体
    /// @param contentType      响应 Content-Type
    /// @param closeConnection  是否在响应后强制关闭连接
    /// @param async            true 表示异步模式，body 在稍后通过 sendAsyncResponse 设置
    struct HandlerResult
    {
        int statusCode = 200;
        std::string reasonPhrase = "OK";
        std::string body;
        std::string contentType = "text/plain; charset=utf-8";
        bool closeConnection = false;
        bool async = false;
    };

    using RequestHandler = std::function<HandlerResult(protocol::HttpRequest &, TcpConnection &)>;

    TcpConnection(const TcpConnection &) = delete;
    TcpConnection &operator=(const TcpConnection &) = delete;

    /// @brief 创建 TcpConnection（禁用直接构造）
    /// @param fd     连接 fd（构造后所有权转移给 Channel）
    /// @param evLoop 所属 EventLoop
    /// @return shared_ptr<TcpConnection>
    static std::shared_ptr<TcpConnection> create(int fd, core::EventLoop *evLoop);

    ~TcpConnection();

    /// @brief 创建 Channel 并注册到 EventLoop
    /// @return true 注册成功，false 注册失败
    bool init();

    // 配置
    void setKeepAliveEnabled(bool enabled);
    void setKeepAlivePolicy(uint32_t maxRequests, uint32_t idleTimeoutMs);
    void setRequestHandler(RequestHandler handler);
    void setCloseCallback(std::function<void(int)> closeCb);

    // IO 事件处理（Channel 回调入口）
    void handleRead();
    void handleWrite();
    void handleClose();

    // Keep-Alive 与状态查询
    bool shouldCloseForIdle(int64_t nowMs) const;
    bool isDisconnected() const;

    // 异步响应回写
    void sendAsyncResponse(const HandlerResult &result);

    // 查询
    int fd() const;
    const std::string &name() const;
    core::EventLoop *getLoop() const;

  private:
    enum State
    {
        kConnecting,
        kConnected,
        kDisconnecting,
        kDisconnected
    };

    TcpConnection(int fd, reactor::core::EventLoop *evLoop, std::string name);

    void destory_();
    void onChannelDestroyed_();
    bool isParseWaitTimeout_() const;
    void appendSimpleResponse_(int statusCode, std::string_view reasonPhrase,
                               std::string_view body, std::string_view contentType);
    void queueSimpleResponse_(int statusCode, std::string_view reasonPhrase);

  private:
    int fd_;
    std::atomic<State> state_;

    // 非拥有
    core::EventLoop *loop_;
    net::Channel *channel_;

    std::string name_;
    base::Buffer readBuffer_;
    base::Buffer writeBuffer_;

    bool asyncPending_ = false;

    std::unique_ptr<protocol::HttpRequest> inFlightRequest_;
    std::unique_ptr<protocol::HttpRequest> request_;
    std::unique_ptr<protocol::HttpResponse> response_;
    std::chrono::steady_clock::time_point parseWaitStart_;
    std::chrono::steady_clock::time_point requestStartTime_;
    bool parseWaiting_ = false;
    bool requestTimingActive_ = false;
    bool keepAliveEnabled_ = false;
    bool keepAliveRequest_ = false;
    uint32_t maxKeepAliveRequests_ = 100;
    uint32_t keepAliveIdleTimeoutMs_ = 10000;
    std::atomic<uint64_t> servedRequests_{0};
    std::atomic<int64_t> lastActivityMs_{0};
    int pendingStatusCode_ = 200;
    std::string pendingReasonPhrase_ = "OK";
    std::string pendingBody_;
    std::string pendingContentType_ = "text/plain; charset=utf-8";
    RequestHandler requestHandler_;
    std::function<void(int)> closeCallback_;
};
} // namespace reactor::net
