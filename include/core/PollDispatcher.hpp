#pragma once

#include "core/Dispatcher.hpp"
#include <cstdint>
#include <sys/poll.h>

namespace reactor::core
{

/// @brief 基于 POSIX poll 的事件调度器实现
///
/// 使用堆分配的 pollfd 数组（maxNode_ = 1024），
/// 构造时全部初始化为 fd = -1，析构时 delete[] 释放。
class PollDispatcher : public Dispatcher
{
  public:
    PollDispatcher(EventLoop *evLoop);
    ~PollDispatcher();

    /// @brief 在 fds_ 中找空位（fd==-1）注册，满时返回 kError
    StatusCode add() override;

    /// @brief 在 fds_ 中查找对应 fd 并清空，未找到返回 kNotFound
    StatusCode remove() override;

    /// @brief 在 fds_ 中查找对应 fd 并更新 events，未找到返回 kNotFound
    StatusCode modify() override;

    /// @brief poll 系统调用，映射 POLLIN/POLLOUT/POLLERR
    StatusCode dispatch(int timeout = 2) override;

  private:
    int32_t maxfd_;
    struct pollfd *fds_;
    int32_t maxNode_ = 1024;
};

} // namespace reactor::core
