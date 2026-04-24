#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <functional>

#include "Channel.hpp"
#include "EventLoop.hpp"
#include "Buffer.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

namespace reactor::net{
    class TcpConnection
    {
    public:
        TcpConnection(const TcpConnection&)=delete;
        TcpConnection& operator= (const TcpConnection&) = delete;
        
        static std::unique_ptr<TcpConnection> create(int fd,core::EventLoop* evLoop);

        ~TcpConnection();
        
        //IO处理(Channel回调)        
        void handleRead();
        void handleWrite();
        void handleClose();
        bool init();
        void setKeepAliveEnabled(bool enabled);
        void setKeepAlivePolicy(uint32_t maxRequests, uint32_t idleTimeoutMs);
        void setCloseCallback(std::function<void(int)> closeCb);
        bool shouldCloseForIdle(int64_t nowMs) const;

        int fd()const;
        const std::string& name() const;
        bool isDisconnected() const;

    private:
        enum State {kConnecting, kConnected, kDisconnecting,kDisconnected};
        
        TcpConnection(int fd, reactor::core::EventLoop* evLoop, std::string name);
        
        void destory_();
        void onChannelDestroyed_();
        bool isParseWaitTimeout_() const;
        void appendSimpleResponse_(int statusCode, std::string_view reasonPhrase);
        void queueSimpleResponse_(int statusCode, std::string_view reasonPhrase);

    private:
        int fd_;
        std::atomic<State> state_;

        //非拥有
        core::EventLoop* loop_;
        net::Channel* channel_;


        std::string name_;
        base::Buffer readBuffer_;
        base::Buffer writeBuffer_;

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
        std::function<void(int)> closeCallback_;
    };
}
