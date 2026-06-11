#include "core/EpollDispatcher.hpp"
#include "net/Channel.hpp"
#include "core/EventLoop.hpp"

#include <cstdint>
#include <sys/types.h>
#include <system_error>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <cerrno>

namespace reactor::core
{
    // ====================================================================
    // 构造
    // ====================================================================
    //
    // epoll_create(1) 的 size 参数在 Linux 2.6.8+ 已被忽略，传 1 即可。
    // readyEvents_ 预分配 520 槽，覆盖绝大多数场景下的并发连接数。
    EpollDispatcher::EpollDispatcher(EventLoop *evLoop)
        : Dispatcher(evLoop), epfd_(-1), maxNode_(520), readyEvents_(maxNode_)
    {
        epfd_ = epoll_create(1);
        if (epfd_ == -1)
        {
            throw std::system_error(errno, std::system_category(), "epoll_create failed");
        }
        name_ = "Epoll";
    }

    EpollDispatcher::~EpollDispatcher()
    {
        if (epfd_ != -1)
        {
            close(epfd_);
            epfd_ = -1;
        }
    }

    StatusCode EpollDispatcher::add()
    {
        return epollCtl_(EPOLL_CTL_ADD) == 0 ? StatusCode::kOk : StatusCode::kError;
    }

    StatusCode EpollDispatcher::remove()
    {
        return epollCtl_(EPOLL_CTL_DEL) == 0 ? StatusCode::kOk : StatusCode::kError;
    }

    StatusCode EpollDispatcher::modify()
    {
        return epollCtl_(EPOLL_CTL_MOD) == 0 ? StatusCode::kOk : StatusCode::kError;
    }

    StatusCode EpollDispatcher::dispatch(int timeout)
    {
        const int count = epoll_wait(epfd_, readyEvents_.data(), maxNode_, timeout * 1000);

        if (count < 0)
        {
            if (errno == EINTR)
            {
                return StatusCode::kAgain;
            }
            return StatusCode::kError;
        }
        for (int i = 0; i < count; i++)
        {
            const int fd = readyEvents_[i].data.fd;
            const uint32_t event = readyEvents_[i].events;

            uint32_t ev = 0;
            // 原来的直接判断改为单独处理，原因如下：
            // 1.
            // EPOLLERR/EPOLLHUP通常和EPOLLIN/EPOLLOUT同时出现，直接关闭可能吧原本应该处理的收尾读写吞掉则此处提出来单独表示
            // 2.
            // EPOLLHUB不一定要立即destory,其标识链接状态异常/对端关闭，需要走统一的连接关闭流程，提升到上层解决
            bool has_error = event & (EPOLLERR | EPOLLHUP | EPOLLRDHUP);
            if (event & (EPOLLIN | EPOLLPRI))
            {
                ev |= static_cast<uint32_t>(net::FDEvent::kReadEvent);
            }

            if (event & EPOLLOUT)
            {
                ev |= static_cast<uint32_t>(net::FDEvent::kWriteEvent);
            }

            if (has_error)
            {
                ev |= static_cast<uint32_t>(net::FDEvent::kErrorEvent);
            }

            if (ev)
            {
                evLoop_->active(fd, ev);
            }
        }

        return StatusCode::kOk;
    }

    int32_t EpollDispatcher::epollCtl_(int op)
    {
        struct epoll_event ev;
        ev.data.fd = channel_->getSocket();
        int events = 0;
        if (channel_->getEvent() & static_cast<uint32_t>(net::FDEvent::kReadEvent))
        {
            events |= EPOLLIN;
        }

        if (channel_->getEvent() & static_cast<uint32_t>(net::FDEvent::kWriteEvent))
        {
            events |= EPOLLOUT;
        }

        ev.events = events;
        int ret = epoll_ctl(epfd_, op, channel_->getSocket(), &ev);
        if (ret == -1)
        {
            return -1;
        }
        return 0;
    }
} // namespace reactor::core
// namespace reactor::core
