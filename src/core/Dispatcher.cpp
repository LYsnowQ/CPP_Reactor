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
    Dispatcher::Dispatcher(EventLoop *evLoop) : evLoop_(evLoop), channel_(nullptr)
    {
    }

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
