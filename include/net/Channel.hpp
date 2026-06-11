#pragma once

#include <functional>
#include <cstdint>
#include <type_traits>

namespace reactor::net
{

/// @brief fd 事件类型枚举（位掩码组合）
///
/// kTimeout     = 0x01 — 超时事件
/// kReadEvent   = 0x02 — 可读事件
/// kWriteEvent  = 0x04 — 可写事件
/// kErrorEvent  = 0x08 — 错误事件
enum class FDEvent : uint32_t
{
    kTimeout = 0x01,
    kReadEvent = 0x02,
    kWriteEvent = 0x04,
    kErrorEvent = 0x08
};

/// @brief IO 事件回调封装，绑定 fd 与读写/销毁回调
///
/// Channel 将 fd 的 IO 事件映射到三个 std::function 回调：
///   - readCallback：可读事件触发
///   - writeCallback：可写事件触发
///   - destroyCallback：Channel 析构时触发
///
/// 生命周期由 EventLoop 通过 channelMap_（unique_ptr）管理。
/// 同一 Channel 实例不可拷贝。
///
/// @param fd          目标 fd
/// @param events      初始监听事件（FDEvent 位组合）
/// @param readCallback    可读回调
/// @param writeCallback   可写回调
/// @param destroyCallback 析构回调
///
/// @thread 非线程安全，仅由所属 EventLoop 线程操作
class Channel
{
  public:
    using Callback = std::function<void()>;

    /// @brief 构造 Channel，绑定 fd 与回调
    /// @param fd            要监听的 fd（Channel 析构时会 close 此 fd）
    /// @param events        初始监听事件掩码
    /// @param readCallback  可读事件回调
    /// @param writeCallback 可写事件回调
    /// @param destroyCallback Channel 析构时触发的回调（用于通知上层连接关闭）
    Channel(int fd, FDEvent events, Callback readCallback, Callback writeCallback,
            Callback destroyCallback);

    /// @brief 析构时 close(fd_) 并调用 destroyCallback
    ~Channel();

    /// @brief 启用/禁用写事件监听
    /// @param flag true 添加写事件，false 移除写事件
    void writeEventEnable(bool flag);

    /// @brief 当前是否监听了写事件
    inline bool isWriteEventEnable() const;

    /// @brief 获取当前监听的事件掩码
    uint32_t getEvent() const;

    /// @brief 获取绑定的 fd
    int getSocket() const;

    /// @brief 执行读回调（回调前检查是否为空）
    void readFunc();
    void writeFunc();
    void destroyFunc();

    bool haveReadCallback();
    bool haveWriteCallback();
    bool haveDestroyCallback();

  private:
    int fd_;
    uint32_t events_;
    Callback readCallback_;
    Callback writeCallback_;
    Callback destroyCallback_;
};

bool Channel::isWriteEventEnable() const
{
    return events_ & static_cast<uint32_t>(FDEvent::kWriteEvent);
}

} // namespace reactor::net
