#include "core/Dispatcher.hpp"
#include "core/EpollDispatcher.hpp"
#include "core/PollDispatcher.hpp"
#include "core/SelectDispatcher.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

namespace reactor::core
{
    // ====================================================================
    // 构造
    // ====================================================================
    //
    // 空实现：evLoop_ 由参数赋值，channel_ 初始为 nullptr。
    // setChannel 在每次 add/remove/modify 前被调用，因此构造时无需设置。
    Dispatcher::Dispatcher(EventLoop *evLoop) : evLoop_(evLoop), channel_(nullptr)
    {
    }

    // ====================================================================
    // 工厂：createDispatcher
    // ====================================================================
    //
    // 根据 type 分支创建具体子类。
    // 新增后端类型时需在此处添加分支，并更新 DispatcherType 枚举。
    std::unique_ptr<Dispatcher> createDispatcher(EventLoop *evLoop, DispatcherType type)
    {
        switch (type)
        {
        case DispatcherType::kEpoll:
            return std::make_unique<EpollDispatcher>(evLoop);
        case DispatcherType::kPoll:
            return std::make_unique<PollDispatcher>(evLoop);
        case DispatcherType::kSelect:
            return std::make_unique<SelectDispatcher>(evLoop);
        default:
            return nullptr;
        }
    }

    // ====================================================================
    // 字符串 → 枚举转换
    // ====================================================================
    //
    // 先转小写再匹配，使命令行输入的 "Epoll"/"EPOLL"/"epoll" 均能正确识别。
    DispatcherType dispatcherTypeFromString(std::string_view name, DispatcherType fallback)
    {
        std::string value(name);
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (value == "epoll")
        {
            return DispatcherType::kEpoll;
        }
        if (value == "poll")
        {
            return DispatcherType::kPoll;
        }
        if (value == "select")
        {
            return DispatcherType::kSelect;
        }
        return fallback;
    }

    const char *dispatcherTypeToString(DispatcherType type)
    {
        switch (type)
        {
        case DispatcherType::kEpoll:
            return "epoll";
        case DispatcherType::kPoll:
            return "poll";
        case DispatcherType::kSelect:
            return "select";
        default:
            return "unknown";
        }
    }
} // namespace reactor::core
// namespace reactor::core
