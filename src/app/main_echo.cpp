// ====================================================================
// main_echo.cpp — 纯 TCP Echo 服务器（压测基准用）
//
// 不做 HTTP 解析、不做字符串拼接、不读写文件。
// 核心逻辑：收到什么字节，原样写回。
//
// 使用项目相同的 EventLoop/Channel/IOThreadPool 基础设施，
// 保证与主服务器可比性。
//
// 用法: ./main_echo <port> <io_threads>
// 默认: ./main_echo 8080 12
// ====================================================================

#include <iostream>
#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <csignal>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "core/EventLoop.hpp"
#include "net/Channel.hpp"
#include "net/IOThreadPool.hpp"

// ====================================================================
// wrk 兼容的 HTTP 响应头（预构建，零开销）
// 每次请求读入的数据直接丢弃，只回写固定 HTTP 200 空响应。
// 这样 wrk 可以正常压测，但服务器不做 HTTP 解析。
// ====================================================================
static const std::string kHttpOkResponse =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 0\r\n"
    "Connection: keep-alive\r\n"
    "\r\n";

// ====================================================================
// 每个连接一个 EchoContext，new/delete + fd 索引数组（无锁并发安全）
// ====================================================================
struct EchoContext {
    bool writePending = false;
    reactor::net::Channel* channel = nullptr;
    reactor::core::EventLoop* loop = nullptr;
};

static constexpr int kMaxFds = 65536;
static EchoContext* s_ctx[kMaxFds] = {};  // fd → EchoContext*
static std::atomic<bool> s_running{true};

// ====================================================================
// 关闭连接
// ====================================================================
static void closeConn_(int fd) {
    if (fd < 0 || fd >= kMaxFds) return;
    EchoContext* ctx = s_ctx[fd];
    if (!ctx) return;
    s_ctx[fd] = nullptr;

    if (ctx->loop && ctx->channel) {
        ctx->loop->destroyTask(fd);
        ctx->channel = nullptr;
    }
    delete ctx;
}

// ====================================================================
// 创建 EchoContext 并注册到 EventLoop
// ====================================================================
static bool registerConn_(int cfd, reactor::core::EventLoop* loop) {
    if (cfd < 0 || cfd >= kMaxFds) {
        close(cfd);
        return false;
    }
    auto* ctx = new EchoContext();
    ctx->writePending = false;
    ctx->channel = nullptr;
    ctx->loop = loop;
    s_ctx[cfd] = ctx;

    auto readCb = [cfd]() {
        EchoContext* ctx = s_ctx[cfd];
        if (!ctx) return;

        char buf[65536];
        ssize_t n = read(cfd, buf, sizeof(buf));
        if (n > 0) {
            if (!ctx->writePending) {
                ctx->writePending = true;
                if (ctx->channel) {
                    ctx->channel->writeEventEnable(true);
                    ctx->loop->addTask(ctx->channel->getSocket(), reactor::core::ChannelOP::MODIFY);
                }
            }
        } else if (n == 0) {
            closeConn_(cfd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            closeConn_(cfd);
        }
    };

    auto writeCb = [cfd]() {
        EchoContext* ctx = s_ctx[cfd];
        if (!ctx) return;
        if (!ctx->writePending) return;

        ssize_t n = write(cfd, kHttpOkResponse.data(), kHttpOkResponse.size());
        if (n > 0) {
            ctx->writePending = false;
            if (ctx->channel) {
                ctx->channel->writeEventEnable(false);
                ctx->loop->addTask(ctx->channel->getSocket(), reactor::core::ChannelOP::MODIFY);
            }
        } else if (n < 0 && errno != EAGAIN) {
            closeConn_(cfd);
        }
    };

    auto destroyCb = [cfd]() {
        EchoContext* ctx = s_ctx[cfd];
        if (ctx) {
            ctx->channel = nullptr;
        }
    };

    auto ch = std::make_unique<reactor::net::Channel>(
        cfd,
        reactor::net::FDEvent::kReadEvent,
        std::move(readCb),
        std::move(writeCb),
        std::move(destroyCb));

    ctx->channel = ch.get();
    loop->addTask(std::move(ch), reactor::core::ChannelOP::ADD);
    return true;
}

// ====================================================================
// main
// ====================================================================
int main(int argc, char** argv) {
    const int port = (argc > 1) ? std::atoi(argv[1]) : 8080;
    const int threads = (argc > 2) ? std::atoi(argv[2]) : 12;

    // 忽略 SIGPIPE，防止 write 到关闭的连接时进程退出
    std::signal(SIGPIPE, SIG_IGN);

    // 创建 IO 线程池
    auto pool = std::make_unique<reactor::net::IOThreadPool>(
        static_cast<uint32_t>(threads),
        reactor::core::DispatcherType::kEpoll);
    pool->start();

    // 创建监听 socket
    int lfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (lfd < 0) {
        std::cerr << "[echo] socket failed" << std::endl;
        return 1;
    }

    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(lfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[echo] bind failed" << std::endl;
        close(lfd);
        return 1;
    }

    if (listen(lfd, 128) < 0) {
        std::cerr << "[echo] listen failed" << std::endl;
        close(lfd);
        return 1;
    }

    std::cout << "[echo] listening on port " << port
              << " with " << threads << " worker threads (epoll)"
              << std::endl;

    // Accept 循环
    while (s_running.load()) {
        int cfd = accept4(lfd, nullptr, nullptr, SOCK_NONBLOCK);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            break;
        }

        // TCP_NODELAY：禁用 Nagle，echo 场景需要最低延迟
        int flag = 1;
        setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        reactor::core::EventLoop* loop = pool->getNextLoop();
        if (!loop) {
            close(cfd);
            continue;
        }

        registerConn_(cfd, loop);
    }

    // 清理
    pool->stop();
    close(lfd);
    std::cout << "[echo] shutdown" << std::endl;
    return 0;
}
