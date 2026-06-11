#include "net/Channel.hpp"
#include <cstdint>
#include <unistd.h>

namespace reactor::net
{
    // ====================================================================
    // 构造
    // ====================================================================
    //
    // 所有回调通过 std::move 转移所有权。
    // events_ 以 uint32_t 存储 FDEvent 位组合，支持通过 writeEventEnable 运行时修改。
    Channel::Channel(int fd, FDEvent events, Callback readCallback, Callback writeCallback,
                     Callback destroyCallback)
        : fd_(fd), events_(static_cast<uint32_t>(events)), readCallback_(std::move(readCallback)),
          writeCallback_(std::move(writeCallback)), destroyCallback_(std::move(destroyCallback))
    {
    }

    // ====================================================================
    // 析构
    // ====================================================================
    //
    // 析构时 close(fd_)，确保 fd 不会被遗漏关闭。
    // 然后调用 destroyCallback，通知上层（如 TcpConnection）连接已断开。
    // 注意：destroyCallback 执行时 Channel 成员仍有效（尚未销毁），
    // 上层可在回调中做清理记录，但不应再操作此 Channel。
    Channel::~Channel()
    {
        if (fd_ >= 0)
        {
            close(fd_);
        }
        destroyFunc();
    }

    bool Channel::haveReadCallback()
    {
        return readCallback_ ? true : false;
    }

    bool Channel::haveWriteCallback()
    {
        return writeCallback_ ? true : false;
    }

    bool Channel::haveDestroyCallback()
    {
        return destroyCallback_ ? true : false;
    }

    void Channel::readFunc()
    {
        if (readCallback_)
            readCallback_();
    }

    void Channel::writeFunc()
    {
        if (writeCallback_)
            writeCallback_();
    }

    void Channel::destroyFunc()
    {
        if (destroyCallback_)
            destroyCallback_();
    }

    void Channel::writeEventEnable(bool flag)
    {
        if (flag)
        {
            events_ |= static_cast<uint32_t>(FDEvent::kWriteEvent);
        }
        else
        {
            events_ &= ~static_cast<uint32_t>(FDEvent::kWriteEvent);
        }
    }

    uint32_t Channel::getEvent() const
    {
        return events_;
    }

    int Channel::getSocket() const
    {
        return fd_;
    }
} // namespace reactor::net
// namespace reactor::net
