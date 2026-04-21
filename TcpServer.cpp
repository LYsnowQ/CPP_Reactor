#include <cstdint>
#include <memory>
#include <system_error>

#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "TcpConnection.hpp"
#include "TcpServer.hpp"    



reactor::net::TcpServer::TcpServer(uint16_t port,uint32_t maxThread)
:lfd_(-1),port_(port),baseLoop_(nullptr)
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
    threadPool_ = std::make_unique<reactor::net::IOThreadPool>(maxThread);
}


void reactor::net::TcpServer::run(TcpServerMode mode)
{
    threadPool_->start();
    int ret = listen(lfd_,128);
    if(ret == -1)
    {
        throw std::system_error(
                errno,
                std::system_category(),
                "listen failed"
                );
    }
    if(mode == TcpServerMode::kChiledThreadMode)
    {//TODO：后续修改
        accepter_ = std::thread(std::bind(&TcpServer::acceptConnection,this));
    }
    else if(mode == TcpServerMode::kMianThreadMode)
    {//TODO：后续修改
        acceptConnection();
    }
}

void reactor::net::TcpServer::acceptConnection()
{
    while(1)
    {
        int cfd = accept(lfd_,nullptr,nullptr);
        reactor::core::EventLoop* evloop = threadPool_->getNextLoop();
        conns_.emplace(cfd,reactor::net::TcpConnection::create(cfd,evloop));
       //TODO: baseLoop_->processTaskQ();
    }
}
