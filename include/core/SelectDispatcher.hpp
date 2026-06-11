#pragma once

#include "core/EventLoop.hpp"
#include "core/Dispatcher.hpp"
#include <cstdint>
#include <sys/select.h>

namespace reactor::core
{

/// @brief 基于 POSIX select 的事件调度器实现
///
/// 受限于 FD_SETSIZE（通常 1024），maxSize_ 固定为 1024。
/// 无动态资源，无需析构函数。
class SelectDispatcher : public Dispatcher
{
  public:
    SelectDispatcher(EventLoop *evLoop);

    /// @brief FD_SET，fd >= maxSize_ 时返回 kError
    StatusCode add() override;

    /// @brief FD_CLR
    StatusCode remove() override;

    /// @brief 先 clear 再 set
    StatusCode modify() override;

    /// @brief select 系统调用，使用 fd_set 副本
    StatusCode dispatch(int timeout = 2) override;

  private:
    void setFdSet_();
    void clearFdSet_();

  private:
    fd_set readSet_;
    fd_set writeSet_;
    int32_t maxSize_ = 1024;
};

} // namespace reactor::core
