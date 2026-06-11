#pragma once
#include "net/Channel.hpp"
#include "core/CoreStatus.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace reactor::core
{
    class EventLoop;

    /// @brief IO 复用调度器后端类型
    enum class DispatcherType : uint8_t
    {
        kEpoll,
        kPoll,
        kSelect
    };

    /// @brief IO 多路复用调度器抽象基类
    ///
    /// 子类实现 add/remove/modify/dispatch 四个纯虚接口。
    /// 生命周期由 EventLoop 通过 unique_ptr 管理，不拥有 Channel 或 EventLoop。
    ///
    /// @param evLoop 所属 EventLoop（非拥有指针，由调用方保证生命周期长于 Dispatcher）
    class Dispatcher
    {
      public:
        Dispatcher(EventLoop *evLoop);
        virtual ~Dispatcher() = default;

        /// @brief 将 channel_->fd 注册到 IO 复用后端
        /// @retval kOk    注册成功
        /// @retval kError 注册失败（如 epoll_ctl 返回 -1）
        /// @pre 调用前须通过 setChannel 设置目标 Channel
        virtual StatusCode add() = 0;

        /// @brief 从 IO 复用后端移除 channel_->fd
        /// @retval kOk    移除成功
        /// @retval kError 移除失败
        virtual StatusCode remove() = 0;

        /// @brief 修改 channel_->fd 的监听事件
        /// @retval kOk    修改成功
        /// @retval kError 修改失败
        virtual StatusCode modify() = 0;

        /// @brief 等待 IO 事件就绪并分发给 EventLoop
        /// @param timeout 超时秒数（子类转换为毫秒）
        /// @retval kOk    正常返回（可能 0 个事件）
        /// @retval kAgain 被信号中断（EINTR），可重试
        /// @retval kError 发生不可恢复错误
        virtual StatusCode dispatch(int timeout = 2) = 0;

        /// @brief 设置当前操作的 Channel
        /// @param channel 非拥有指针，仅用于 add/remove/modify 时读取 fd 和事件掩码
        inline void setChannel(net::Channel *channel);

      protected:
        std::string name_ = std::string();
        EventLoop *evLoop_; // 观察者，生命周期由tcpserver掌管不用智能指针，其拥有dispatcher
        net::Channel *channel_;
    };

    /// @brief 根据 DispatcherType 创建具体子类实例
    /// @param evLoop 新 Dispatcher 所属的 EventLoop
    /// @param type   后端类型
    /// @return unique_ptr 到 Dispatcher，type 无效时返回 nullptr
    std::unique_ptr<Dispatcher> createDispatcher(EventLoop *evLoop, DispatcherType type);

    /// @brief 将字符串转换为 DispatcherType（大小写不敏感）
    /// @param name     字符串（"epoll"/"poll"/"select"）
    /// @param fallback 无法识别时返回的默认值
    DispatcherType dispatcherTypeFromString(std::string_view name,
                                            DispatcherType fallback = DispatcherType::kEpoll);

    /// @brief 将 DispatcherType 转换为可读字符串
    const char *dispatcherTypeToString(DispatcherType type);

    // 内部使用时不对其进行创建和销毁，则在此处我们使用原始指针，其销毁交给channelMap_
    void Dispatcher::setChannel(net::Channel *channel)
    {
        channel_ = channel;
    }
} // namespace reactor::core
