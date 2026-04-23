#include <cstdlib>
#include <exception>
#include <iostream>

#include <unistd.h>

#include "Dispatcher.hpp"
#include "TcpServer.hpp"
#include "spdlog/spdlog.h"

int main(int argc, const char** argv)
{
    if(argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <resource_path> [dispatcher: epoll|poll|select] [threads]\n";
        return -1;
    }

    const auto port = static_cast<uint16_t>(std::atoi(argv[1]));
    if(chdir(argv[2]) != 0)
    {
        std::cerr << "资源目录切换失败: " << argv[2] << "\n";
        return -1;
    }

    const auto dispatcherType = (argc >= 4)
                                    ? reactor::core::dispatcherTypeFromString(argv[3], reactor::core::DispatcherType::kEpoll)
                                    : reactor::core::DispatcherType::kEpoll;
    const auto maxThreads = (argc >= 5) ? static_cast<uint32_t>(std::atoi(argv[4])) : 4U;

    spdlog::set_level(spdlog::level::debug);
    spdlog::debug(
        "资源定向成功，开始启动服务器，dispatcher={}, threads={}",
        reactor::core::dispatcherTypeToString(dispatcherType),
        maxThreads);

    try
    {
        reactor::net::TcpServer server(port, maxThreads, dispatcherType);
        const auto status = server.run();
        return (status == reactor::core::StatusCode::kOk) ? 0 : -1;
    }
    catch(const std::exception& ex)
    {
        std::cerr << "服务器初始化/运行异常: " << ex.what() << "\n";
        return -1;
    }
}
