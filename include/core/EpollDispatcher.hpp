#pragma once

#include "core/EventLoop.hpp"
#include "core/Dispatcher.hpp"
#include <cstdint>
#include <vector>
#include <sys/epoll.h>

namespace reactor::core
{

/// @brief 基于 Linux epoll 的事件调度器实现
///
/// 构造时调用 epoll_create 创建 epoll 实例，析构时关闭 epfd。
/// readyEvents_ 预分配 maxNode_（520）个槽位。
///
/// @throw std::system_error epoll_create 失败时抛出
class EpollDispatcher : public Dispatcher
{
  public:
    EpollDispatcher(EventLoop *evLoop);
    ~EpollDispatcher();

    /// @brief 注册 channel_->fd 到 epoll 实例
    StatusCode add() override;

    /// @brief 从 epoll 实例移除 channel_->fd
    StatusCode remove() override;

    /// @brief 修改 channel_->fd 的监听事件
    StatusCode modify() override;

    /// @brief epoll_wait 等待事件就绪
    ///
    /// 事件映射规则：
    ///   EPOLLIN|EPOLLPRI → kReadEvent
    ///   EPOLLOUT → kWriteEvent
    ///   EPOLLERR|EPOLLHUP|EPOLLRDHUP → kErrorEvent
    /// 错误事件与读写事件不互斥，全部映射后通过 evLoop_->active 一次性上报。
    ///
    /// @param timeout 超时秒数（转换为毫秒传入 epoll_wait）
    /// @retval kOk    正常返回
    /// @retval kAgain 被 EINTR 中断
    /// @retval kError epoll_wait 返回不可恢复错误
    StatusCode dispatch(int timeout = 2) override;

  private:
    int32_t epollCtl_(int32_t op);

  private:
    int epfd_;
    int maxNode_;
    std::vector<epoll_event> readyEvents_;
};

} // namespace reactor::core
