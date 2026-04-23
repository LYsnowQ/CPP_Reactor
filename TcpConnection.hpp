#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include <string_view>

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

        int fd()const;
        const std::string& name() const;
        bool isDisconnected() const;

    private:
        enum State {kConnecting, kConnected, kDisconnecting,kDisconnected};
        
        TcpConnection(int fd, reactor::core::EventLoop* evLoop, std::string name);
        
        bool init_();
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
    };
}
