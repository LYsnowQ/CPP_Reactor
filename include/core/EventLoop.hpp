#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <sys/socket.h>
#include <queue>
#include <thread>
#include <map>
#include <mutex>
#include <atomic>
#include <functional>
#include "core/CoreStatus.hpp"
#include "net/Channel.hpp"
#include "core/Dispatcher.hpp"

namespace reactor::core
{

    /// @brief Channel 操作类型
    /// ADD: 添加 Channel（转移 unique_ptr 所有权）
    /// DELETE: 移除 Channel（仅需 fd）
    /// MODIFY: 修改监听事件（仅需 fd）
    enum class ChannelOP : uint8_t
    {
        ADD,
        DELETE,
        MODIFY
    };

    class Dispatcher;

    /// @brief 事件循环核心
    ///
    /// 职责：
    ///   1. 管理 Channel 生命周期（channelMap_ 持有 unique_ptr）
    ///   2. 任务队列调度（同一线程非 DELETE 同步处理，跨线程唤醒处理）
    ///   3. 将 IO 事件分发给对应 Channel 的回调
    ///   4. 跨线程回调投递（post/processTaskQ）
    ///
    /// 线程绑定：一个 EventLoop 只应被一个线程驱动（run 的调用线程）。
    /// 非所属线程投递任务需通过 addTask/post + socketpair 唤醒机制。
    ///
    /// @thread 构造线程与 run 线程必须一致
    /// @throw std::system_error socketpair 创建失败
    /// @throw std::runtime_error Dispatcher 创建失败
    class EventLoop
    {
      public:
        /// @brief 任务队列元素
        struct ChannelElement
        {
            ChannelOP type;                        ///< 操作类型
            std::unique_ptr<net::Channel> channel; ///< ADD 时持有所有权
            int fd;                                ///< DELETE/MODIFY 时使用
        };

        /// @brief 跨线程回调类型（SQL 异步结果回写等场景）
        using Callback = std::function<void()>;

        EventLoop();

        EventLoop(std::string name, DispatcherType type = DispatcherType::kEpoll);

        ~EventLoop();

        /// @brief 启动事件循环，阻塞当前线程
        /// @retval kOk    正常退出（shutdown 触发）
        /// @retval kError dispatcher->dispatch 返回 kError
        /// @thread 必须由构造时绑定的线程调用
        StatusCode run();

        /// @brief 向 EventLoop 投递带 Channel 所有权的任务
        /// @param channel ADD 时转移所有权；DELETE/MODIFY 时传 nullptr
        /// @param type 操作类型
        /// @retval kOk 入队成功
        /// @thread safe
        StatusCode addTask(std::unique_ptr<net::Channel> channel, ChannelOP type);

        /// @brief 向 EventLoop 投递仅基于 fd 的任务（DELETE/MODIFY）
        /// @param fd  目标 fd
        /// @param type 操作类型（不可为 ADD）
        /// @retval kOk      入队成功
        /// @retval kInvalid fd < 0 或 type == ADD
        /// @thread safe
        StatusCode addTask(int fd, ChannelOP type);

        /// @brief 快捷方式：投递 DELETE 任务
        /// @param fd 目标 fd
        /// @thread safe
        StatusCode destroyTask(int fd);

        /// @brief 由 Dispatcher 调用，上报 IO 事件
        ///
        /// 事件处理顺序：错误事件优先 → 读事件 → 写事件。
        /// 读回调执行后重新检查 Channel 存活状态（回调可能同步触发 DELETE）。
        ///
        /// @param fd    就绪的 fd
        /// @param event 事件掩码（FDEvent 位组合）
        /// @retval kOk      处理完成
        /// @retval kInvalid fd < 0
        /// @retval kNotFound channelMap 中无此 fd
        /// @thread 仅由所属 EventLoop 线程调用
        StatusCode active(int fd, uint32_t event);

        /// @brief 处理任务队列与回调队列
        ///
        /// 加锁交换到本地队列后依次处理 Channel 任务和回调队列。
        /// 回调队列在任务队列之后处理，保证 Channel 状态变更先于回调执行。
        StatusCode processTaskQ();

        /// @brief 投递回调到 EventLoop 线程异步执行
        /// @param cb 回调函数
        /// @thread safe
        void post(Callback cb);

        /// @brief 请求退出事件循环
        /// @thread safe
        void shutdown();

      private:
        void taskWakeup_();
        void readLocalMessage_();

        StatusCode add_(std::unique_ptr<net::Channel> channel);
        StatusCode remove_(int fd);
        StatusCode modify_(int fd);

      private:
        std::unique_ptr<Dispatcher> dispatcher_;

        // 任务队列
        std::queue<ChannelElement> taskQ_;

        std::string threadName_;
        std::thread::id threadID_;

        // 映射与互斥锁:
        std::map<int, std::unique_ptr<net::Channel>> channelMap_;
        std::mutex mutex_;
        std::atomic<bool> isQuit_{true};

        int socketPair_[2]; // 存储由 socketpair 初始化的本地通信 fd
        std::queue<Callback> callbackQueue_;//SQL回调队列
    };

} // namespace reactor::core
