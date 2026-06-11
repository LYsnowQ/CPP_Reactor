#include "core/EventLoop.hpp"
#include "net/Channel.hpp"
#include "core/Dispatcher.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <string.h>
#include <cerrno>
#include <fcntl.h>
#include <stdexcept>

namespace reactor::core
{
    EventLoop::EventLoop() : EventLoop("MainThread")
    {
    }

    // ====================================================================
    // 构造
    // ====================================================================
    //
    // socketPair 用于跨线程唤醒：
    // 其他线程投递任务时向 socketPair[1] 写一个字节，
    // 打断 dispatcher 的阻塞等待，使 EventLoop 能及时处理新任务。
    //
    // socketPair 两端均设为 O_NONBLOCK，防止跨线程 write 阻塞。
    //
    // 创建本地读 Channel（监听 socketPair[0] 的读事件），
    // 通过 addTask ADD 注册到 channelMap_，使事件循环能响应唤醒信号。
    EventLoop::EventLoop(std::string name, DispatcherType type)
        : threadName_(name), threadID_(std::this_thread::get_id())
    {
        socketPair_[0] = -1;
        socketPair_[1] = -1;
        int32_t ret = socketpair(AF_UNIX, SOCK_STREAM, 0, socketPair_);
        if (ret == -1)
        {
            throw std::system_error(errno, std::system_category(),
                                    "socketpair failed in EventLoop");
        }

        // 线程唤醒使用非阻塞，防止跨线程阻塞
        int flags0 = fcntl(socketPair_[0], F_GETFL, 0);
        int flags1 = fcntl(socketPair_[1], F_GETFL, 0);
        if (flags0 != -1)
        {
            fcntl(socketPair_[0], F_SETFL, flags0 | O_NONBLOCK);
        }
        if (flags1 != -1)
        {
            fcntl(socketPair_[1], F_SETFL, flags1 | O_NONBLOCK);
        }
        dispatcher_ = createDispatcher(this, type);
        if (!dispatcher_)
        {
            throw std::runtime_error("create dispatcher failed");
        }

        auto obj = std::bind(&EventLoop::readLocalMessage_, this);
        std::unique_ptr<net::Channel> channel = std::make_unique<net::Channel>(
            socketPair_[0], net::FDEvent::kReadEvent, obj, nullptr, nullptr);
        addTask(std::move(channel), ChannelOP::ADD);
    }

    EventLoop::~EventLoop()
    {
        if (socketPair_[1] >= 0)
        {
            close(socketPair_[1]);
        }
    }

    // ====================================================================
    // 跨线程唤醒
    // ====================================================================
    //
    // taskWakeup_ 写入单字节 'w' 唤醒目标线程。
    // readLocalMessage_ 循环读取直到 EAGAIN，消费所有堆积的唤醒信号。

    // 本地写数据
    void EventLoop::taskWakeup_()
    {
        char msg = 'w';
        ssize_t n = write(socketPair_[1], &msg, sizeof(msg));
        (void)n;
    }

    // 本地读数据
    void EventLoop::readLocalMessage_()
    {
        char buf[256];
        while (true)
        { // 避免信号堆积
            ssize_t n = read(socketPair_[0], buf, sizeof(buf));
            if (n <= 0)
            {
                break;
            }
        }
    }

    // ====================================================================
    // 主循环
    // ====================================================================
    //
    // isQuit_ 初始为 true，run 中设为 false，
    // 确保构造后到 run 调用前 shutdown 不会误退出。
    // 每轮循环：dispatch 等待事件 → processTaskQ 处理任务+回调。
    StatusCode EventLoop::run()
    {
        isQuit_.store(false); // 延迟启动非初始化时启动
        if (std::this_thread::get_id() != threadID_)
        {
            return StatusCode::kError;
        }

        while (!isQuit_)
        {
            if (dispatcher_->dispatch() == StatusCode::kError)
            {
                return StatusCode::kError;
            }
            processTaskQ();
        }

        return StatusCode::kOk;
    }

    // ====================================================================
    // 事件分发
    // ====================================================================
    //
    // 错误事件优先：直接 remove 销毁 Channel。
    // 读事件执行后重查 channelMap_：
    //   读回调可能通过同线程 processTaskQ 触发 DELETE 导致 Channel 被销毁，
    //   若不重查，后续写事件处理将解引用悬空指针。
    StatusCode EventLoop::active(int fd, uint32_t event)
    {
        if (fd < 0)
        {
            return StatusCode::kInvalid;
        }

        auto it = channelMap_.find(fd);
        if (it == channelMap_.end() || !it->second)
        {
            return StatusCode::kNotFound;
        }
        auto *channel = it->second.get();

        if (event & static_cast<uint32_t>(net::FDEvent::kErrorEvent))
        {
            return remove_(fd);
        }

        if (event & static_cast<uint32_t>(net::FDEvent::kReadEvent) && channel->haveReadCallback())
        {
            channel->readFunc();
            // 读回调可能通过同线程投递并立即处理 DELETE。
            // 重新检查 channel 是否存在，避免解引用悬空指针。
            it = channelMap_.find(fd);
            if (it == channelMap_.end() || !it->second)
            {
                return StatusCode::kOk;
            }
            channel = it->second.get();
        }

        if (event & static_cast<uint32_t>(net::FDEvent::kWriteEvent) &&
            channel->haveWriteCallback())
        {
            channel->writeFunc();
        }

        return StatusCode::kOk;
    }

    // ====================================================================
    // 任务投递
    // ====================================================================
    //
    // 同一线程投递非 DELETE 任务直接同步 processTaskQ：
    //   减少一次 socketpair 唤醒开销。
    // DELETE 无论是否同线程均仅入队 + 跨线程唤醒：
    //   避免 delete 过程中 dispatch 循环正在 active 遍历 channelMap_ 导致迭代器失效。
    StatusCode EventLoop::addTask(std::unique_ptr<net::Channel> channel, ChannelOP type)
    {
        {
            std::lock_guard<std::mutex> lk(mutex_);
            taskQ_.push(ChannelElement{type, std::move(channel), -1});
        }
        // 当前线程直接处理非 DELETE 任务，减少唤醒开销。
        // DELETE 即使同线程也只入队 + 跨线程唤醒：
        // 若 dispatch 循环正在 active 中遍历 channelMap_，
        // 同步 processTaskQ 递归进入 remove_ 会导致迭代器失效。
        const bool sameThread = (threadID_ == std::this_thread::get_id());
        if (sameThread && type != ChannelOP::DELETE)
        {
            // 当前线程直接处理
            processTaskQ();
        }
        else if (!sameThread)
        {
            // 跨线程需要唤醒
            taskWakeup_();
        }
        return StatusCode::kOk;
    }

    StatusCode EventLoop::addTask(int fd, ChannelOP type)
    {
        if (fd < 0)
        {
            return StatusCode::kInvalid;
        }
        if (type == ChannelOP::ADD)
        {
            return StatusCode::kInvalid;
        }

        {
            std::lock_guard<std::mutex> lk(mutex_);
            taskQ_.push(ChannelElement{type, nullptr, fd});
        }
        // 当前线程直接处理非 DELETE 任务，减少唤醒开销。
        // DELETE 即使同线程也只入队 + 跨线程唤醒：
        // 若 dispatch 循环正在 active 中遍历 channelMap_，
        // 同步 processTaskQ 递归进入 remove_ 会导致迭代器失效。
        const bool sameThread = (threadID_ == std::this_thread::get_id());
        if (sameThread && type != ChannelOP::DELETE)
        {
            // 当前线程直接处理
            processTaskQ();
        }
        else if (!sameThread)
        {
            // 跨线程需要唤醒
            taskWakeup_();
        }
        return StatusCode::kOk;
    }

    StatusCode EventLoop::destroyTask(int fd)
    {
        return addTask(fd, ChannelOP::DELETE);
    }

    // ====================================================================
    // 任务与回调队列处理
    // ====================================================================
    //
    // 加锁交换到本地队列处理，缩短锁持有时间。
    //
    // ChannelElement 中，DELETE/MODIFY 优先用 fd 字段，
    // 兼容部分调用方持 Channel 对象投递 DELETE 的历史情况。
    //
    // 回调队列在任务队列之后处理，保证 Channel 状态变更先于回调执行。
    StatusCode EventLoop::processTaskQ()
    {
        std::queue<ChannelElement> localQ;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            std::swap(localQ, taskQ_);
        }

        while (!localQ.empty())
        {
            auto element = std::move(localQ.front());
            localQ.pop();
            if (element.type == ChannelOP::ADD)
            {
                add_(std::move(element.channel));
            }
            else if (element.type == ChannelOP::DELETE)
            {
                int fd = element.fd;
                if (fd < 0 && element.channel)
                {
                    fd = element.channel->getSocket();
                }
                remove_(fd);
            }
            else if (element.type == ChannelOP::MODIFY)
            {
                int fd = element.fd;
                if (fd < 0 && element.channel)
                {
                    fd = element.channel->getSocket();
                }
                modify_(fd);
            }
        }

        //处理回调队列
        {
            std::queue<Callback> localCbs;
            {
                std::lock_guard<std::mutex> lk(mutex_);
                localCbs.swap(callbackQueue_);
            }
            while(!localCbs.empty())
            {
                auto cb = std::move(localCbs.front());
                localCbs.pop();
                cb();
            }
        }
        return StatusCode::kOk;
    }


    void EventLoop::post(Callback cb)
    {
        bool needWakeup = false;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            callbackQueue_.push(std::move(cb));
            needWakeup = (threadID_ != std::this_thread::get_id());
        }
        if(needWakeup)
        {
            taskWakeup_();
        }
    }


    void EventLoop::shutdown()
    {
        isQuit_.store(true);
        taskWakeup_();
    }

    // ====================================================================
    // Channel 注册
    // ====================================================================
    //
    // 若 channelMap_ 中已存在此 fd（重复注册），返回 kError。
    // dispatcher->add 失败时回滚 channelMap_ 的插入，保证状态一致性。
    StatusCode EventLoop::add_(std::unique_ptr<net::Channel> channel)
    {
        int fd = channel->getSocket();

        if (channelMap_.find(fd) == channelMap_.end())
        {
            channelMap_.insert(std::make_pair(fd, std::move(channel)));
            dispatcher_->setChannel(channelMap_[fd].get());
            if (dispatcher_->add() != StatusCode::kOk)
            {
                channelMap_.erase(fd);
                return StatusCode::kError;
            }
            return StatusCode::kOk;
        }

        return StatusCode::kError;
    }

    // ====================================================================
    // Channel 移除
    // ====================================================================
    //
    // dispatcher->remove 成功后从 channelMap_ 擦除（触发 unique_ptr 析构）。
    // 若 remove 失败（如 epoll_ctl DEL 返回 ENOENT），仅记录不擦除。
    StatusCode EventLoop::remove_(int fd)
    {
        if (channelMap_.find(fd) == channelMap_.end())
        {
            return StatusCode::kNotFound;
        }
        dispatcher_->setChannel(channelMap_[fd].get());
        StatusCode ret = dispatcher_->remove();
        if (ret == StatusCode::kOk)
        {
            channelMap_.erase(fd);
        }
        return ret;
    }

    StatusCode EventLoop::modify_(int fd)
    {
        if (channelMap_.find(fd) == channelMap_.end())
        {
            return StatusCode::kNotFound;
        }
        dispatcher_->setChannel(channelMap_[fd].get());
        StatusCode ret = dispatcher_->modify();
        return ret;
    }
} // namespace reactor::core
// namespace reactor::core
