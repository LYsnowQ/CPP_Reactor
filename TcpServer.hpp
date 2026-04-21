#pragma once 

#include <cstdint>
#include <memory>
#include <map>

#include "TcpConnection.hpp"
#include "EventLoop.hpp"
#include "IOThreadPool.hpp"

namespace reactor::net{
    
    enum class TcpServerMode:uint16_t
    {
        kMianThreadMode,
        kChiledThreadMode
    };


    class TcpServer
    {
    public:
        TcpServer(uint16_t port,uint32_t maxThread);
        ~TcpServer();

        void run(TcpServerMode mode = TcpServerMode::kMianThreadMode);

        void acceptConnection();

        void stop();
    private:
        int lfd_;
        uint16_t port_;
        std::thread accepter_;
        std::map<int,std::unique_ptr<TcpConnection>>conns_;
        std::unique_ptr<core::EventLoop> baseLoop_;
        std::unique_ptr<IOThreadPool> threadPool_;
    };
}
