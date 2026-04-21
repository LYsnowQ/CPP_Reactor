#pragma once

#include <memory>
#include <string>

#include "Channel.hpp"
#include "EventLoop.hpp"
#include "Buffer.hpp"
//#include "HttpRequest.hpp"
//#include "HttpResponse.hpp"

//是否每次组装完后自动发送（非一次性发送）
//#define MSG_SEND_AUTO

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

    private:
        enum State {kConnecting, kConnected, kDisconnecting,kDisconnected};
        
        TcpConnection(int fd, reactor::core::EventLoop* evLoop, std::string name);
        
        bool init_();
        void destory_();

    private:
        int fd_;
        State state_;

        //非拥有
        core::EventLoop* loop_;
        net::Channel* channel_;


        std::string name_;
        base::Buffer readBuffer_;
        base::Buffer writeBuffer_;

        //http协议
        //TODO:等待http适配后使用
        //std::unique_ptr<HttpRequest> request_;
        //std::unique_ptr<HttpResponse> response_;
    };
}

