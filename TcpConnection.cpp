#include "TcpConnection.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Metrics.hpp"

#include <cerrno>
#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

#include "spdlog/spdlog.h"

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
        for(char c : in)
        {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    }

    bool shouldKeepAlive_(reactor::net::protocol::HttpRequest& req)
    {
        const auto versionLower = toLowerCopy_(req.version());
        const auto headers = req.getHeader();

        std::string connectionLower;
        for(const auto& kv : headers)
        {
            if(toLowerCopy_(kv.first) == "connection")
            {
                connectionLower = toLowerCopy_(kv.second);
                break;
            }
        }

        if(versionLower == "http/1.0")
        {
            return connectionLower.find("keep-alive") != std::string::npos;
        }
        return connectionLower.find("close") == std::string::npos;
    }
}


std::unique_ptr<reactor::net::TcpConnection> reactor::net::TcpConnection::create(int fd,core::EventLoop* loop)
{
    auto conn = std::unique_ptr<reactor::net::TcpConnection>(
                new TcpConnection(fd,loop,"Connection-"+std::to_string(fd))
                );
    return conn;
}


reactor::net::TcpConnection::TcpConnection(int fd,core::EventLoop* loop,std::string name)
    :fd_(fd),
    state_(kConnecting),
    loop_(loop),
    channel_(nullptr),
    name_(std::move(name)),
    parseWaitStart_(std::chrono::steady_clock::now()),
    requestStartTime_(std::chrono::steady_clock::now()),
    parseWaiting_(false)
{
    lastActivityMs_.store(nowSteadyMs_(), std::memory_order_relaxed);
}

reactor::net::TcpConnection::~TcpConnection()
{
    destory_();
}

int reactor::net::TcpConnection::fd() const
{
    return fd_;
}

const std::string& reactor::net::TcpConnection::name() const
{
    return name_;
}

void reactor::net::TcpConnection::destory_()
{
    channel_ = nullptr;
    state_ = kDisconnected;
}


bool reactor::net::TcpConnection::init()
{
    auto ch = std::make_unique<net::Channel>(
                fd_,
                FDEvent::kReadEvent,
                std::bind(&TcpConnection::handleRead,this),
                std::bind(&TcpConnection::handleWrite,this),
                std::bind(&TcpConnection::onChannelDestroyed_, this)
            );

    channel_ = ch.get();
    loop_->addTask(std::move(ch), core::ChannelOP::ADD);
    state_ = kConnected;
    return true;
}

void reactor::net::TcpConnection::setKeepAliveEnabled(bool enabled)
{
    keepAliveEnabled_ = enabled;
}

void reactor::net::TcpConnection::setKeepAlivePolicy(uint32_t maxRequests, uint32_t idleTimeoutMs)
{
    maxKeepAliveRequests_ = (maxRequests == 0) ? 1U : maxRequests;
    keepAliveIdleTimeoutMs_ = (idleTimeoutMs == 0) ? 1U : idleTimeoutMs;
}

void reactor::net::TcpConnection::setCloseCallback(std::function<void(int)> closeCb)
{
    closeCallback_ = std::move(closeCb);
}

bool reactor::net::TcpConnection::shouldCloseForIdle(int64_t nowMs) const
{
    if(!keepAliveEnabled_)
    {
        return false;
    }
    if(state_.load() != kConnected)
    {
        return false;
    }
    if(keepAliveIdleTimeoutMs_ == 0)
    {
        return false;
    }
    const auto lastMs = lastActivityMs_.load(std::memory_order_relaxed);
    return (nowMs - lastMs) >= static_cast<int64_t>(keepAliveIdleTimeoutMs_);
}

bool reactor::net::TcpConnection::isDisconnected() const
{
    return state_.load() == kDisconnected;
}

void reactor::net::TcpConnection::onChannelDestroyed_()
{
    channel_ = nullptr;
    state_.store(kDisconnected);
    if(closeCallback_)
    {
        closeCallback_(fd_);
    }
}


void reactor::net::TcpConnection::handleRead()
{
    int savedErr = 0;
    const ssize_t n = readBuffer_.readFd(channel_->getSocket(), &savedErr);

    if(n > 0)
    {
        lastActivityMs_.store(nowSteadyMs_(), std::memory_order_relaxed);
        reactor::observability::Metrics::instance().onBytesRead(static_cast<uint64_t>(n));
        if(!requestTimingActive_)
        {
            requestTimingActive_ = true;
            requestStartTime_ = std::chrono::steady_clock::now();
        }

        auto parseResult = protocol::HttpRequest::parseRequest(&readBuffer_, &inFlightRequest_);

        if(parseResult.code == core::StatusCode::kAgain)
        {
            if(!parseWaiting_)
            {
                parseWaiting_ = true;
                parseWaitStart_ = std::chrono::steady_clock::now();
            }
            else if(isParseWaitTimeout_())
            {
                queueSimpleResponse_(408, "Request Timeout");
            }
            // 请求尚未收全，继续等下一批数据/超时后由写事件回包
            return;
        }
        parseWaiting_ = false;

        if(parseResult.code != core::StatusCode::kOk || !parseResult.request)
        {
            inFlightRequest_.reset();
            keepAliveRequest_ = false;
            if(parseResult.tooLarge)
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

        if(channel_)
        {
            channel_->writeEventEnable(true);
        }
        loop_->addTask(channel_->getSocket(),core::ChannelOP::MODIFY);
    }
    else if(n == 0)
    {
        handleClose();
    }
    else if(savedErr != EAGAIN && savedErr != EWOULDBLOCK)
    {
        handleClose();
    }

}

bool reactor::net::TcpConnection::isParseWaitTimeout_() const
{
    constexpr auto kParseWaitTimeout = std::chrono::seconds(5);
    return std::chrono::steady_clock::now() - parseWaitStart_ >= kParseWaitTimeout;
}

void reactor::net::TcpConnection::appendSimpleResponse_(int statusCode, std::string_view reasonPhrase)
{
    reactor::observability::Metrics::instance().onResponseStatus(statusCode);
    if(requestTimingActive_)
    {
        const auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - requestStartTime_)
                                 .count();
        reactor::observability::Metrics::instance().onRequestLatencyUs(static_cast<uint64_t>(latency));
        requestTimingActive_ = false;
    }

    std::string line =
        "HTTP/1.1 " + std::to_string(statusCode) + " " + std::string(reasonPhrase) + "\r\n"
        "Content-Length: 0\r\n"
        "Connection: " + std::string(keepAliveRequest_ ? "keep-alive" : "close") + "\r\n"
        "Server: CPPReactor\r\n"
        "\r\n";
    writeBuffer_.append(line);
}

void reactor::net::TcpConnection::queueSimpleResponse_(int statusCode, std::string_view reasonPhrase)
{
    appendSimpleResponse_(statusCode, reasonPhrase);
    if(channel_)
    {
        channel_->writeEventEnable(true);
    }
    loop_->addTask(channel_->getSocket(),core::ChannelOP::MODIFY);
}


void reactor::net::TcpConnection::handleWrite()
{
    if(writeBuffer_.readableBytes() == 0)
    {
        appendSimpleResponse_(200, "OK");
    }

    const ssize_t n = writeBuffer_.writeFd(fd_);
    if(n > 0)
    {
        lastActivityMs_.store(nowSteadyMs_(), std::memory_order_relaxed);
        reactor::observability::Metrics::instance().onBytesWritten(static_cast<uint64_t>(n));
    }
    if(n < 0 && errno != EAGAIN)
    {
        handleClose();
        return;
    }
    if(writeBuffer_.readableBytes() == 0)
    {
        if(keepAliveRequest_)
        {
            keepAliveRequest_ = false;
            const auto served = servedRequests_.fetch_add(1, std::memory_order_relaxed) + 1;
            request_.reset();
            if(served >= static_cast<uint64_t>(maxKeepAliveRequests_))
            {
                handleClose();
                return;
            }
            if(channel_)
            {
                channel_->writeEventEnable(false);
                loop_->addTask(channel_->getSocket(),core::ChannelOP::MODIFY);
            }
            return;
        }
        handleClose();
    }
}


void reactor::net::TcpConnection::handleClose()
{
    const auto cur = state_.load();
    if(cur == kDisconnected || cur == kDisconnecting)
    {
        return;
    }
    state_.store(kDisconnecting);
    if(channel_)
    {
        loop_->addTask(channel_->getSocket(),core::ChannelOP::DELETE);
    }
    else
    {
        state_.store(kDisconnected);
    }
}


#if 0
struct TcpConnection* tcpConnectionInit(int fd,struct EventLoop* evLoop)
{   
    struct TcpConnection* conn = (struct TcpConnection*)malloc(sizeof(struct TcpConnection));
    conn->evLoop = evLoop;
    conn->readBuffer = bufferInit(10240);
    conn->writeBuffer = bufferInit(10240);
    
    // HTTP 协议对象
    conn->request = httpRequestInit();
    conn->response = httpResponseInit();
    sprintf(conn->name,"Connection-%d",fd);
    conn->channel = channelInit(fd ,ReadEvent, processRead, processWrite, tcpConnectionDestory,conn);
    eventLoopAddTask(evLoop, conn->channel, ADD);

    spdlog::debug(
            "和客户端建立连接,threadName: {}, threadID: {},connName: {}",
            evLoop->threadName,evLoop->threadID,conn->name);

    if(!conn->readBuffer || 
        !conn->writeBuffer ||
        !conn->channel ||
        !conn->evLoop)
    {
        free(conn);
        perror("TcpConnection");
        return NULL;
    }

    return conn;
}
int processWrite(void* arg)
{
    spdlog::debug("开始发送数据（基于事件）....");
    struct TcpConnection* conn = (struct TcpConnection*)arg;

    int count = bufferSendData(conn->writeBuffer, conn->channel->fd);
    if(count > 0)
    {
        if(bufferReadableSize(conn->writeBuffer) == 0)
        {
            //writeEventEnable(conn->channel, false);

            //eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);

            eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
        }
    }

    return 0;
}

int processRead(void* arg)
{
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    int count = bufferSocketRead(conn->readBuffer, conn->channel->fd); 
    
    spdlog::debug("接收到http请求，解析http请求：{}",conn->readBuffer->data+conn->readBuffer->readPos);

    if(count > 0)
    {
        //接收 HTTP 请求，解析 HTTP 请求
        int socket = conn->channel->fd;
#ifdef MSG_SEND_AUTO                
        writeEventEnable(conn->channel,true);
        eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
#endif
        bool flag = parseHttpRequest(conn->request, conn->readBuffer, conn->response, conn->writeBuffer,socket);
        if(!flag)
        {
            //解析失败
            char* errMsg = " Http/1.1 400 Bad Request\r\n\r\n";
            bufferAppendString(conn->writeBuffer, errMsg);
        }
    }
#ifndef MSG_SEND_AUTO
    //断开连接
    eventLoopAddTask(conn->evLoop, conn->channel ,DELETE);
#endif
    return 0;
}


int tcpConnectionDestory(void* arg)
{
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    if(conn == NULL)
    {
        return 0;
    } 
    char name[32] = {0};
    strncpy(name, conn->name, sizeof(name) - 1);
    if(conn->readBuffer && bufferReadableSize(conn->readBuffer) == 0 &&
        conn->writeBuffer && bufferReadableSize(conn->writeBuffer) == 0)
    {
        destoryChannel(conn->evLoop, conn->channel);
        bufferDestory(conn->readBuffer);
        bufferDestory(conn->writeBuffer);
        httpRequestDestory(conn->request);
        httpResponseDestory(conn->response);
        free(conn);
    }
    spdlog::debug("连接断开，释放资源，connName:{}",name);
    return 0;
}
#endif
