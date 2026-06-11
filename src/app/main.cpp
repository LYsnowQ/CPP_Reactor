#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <unistd.h>


#include "core/CoreStatus.hpp"
#include "handler/SqlHandler.hpp"
#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlExecutor.hpp"
#include "persistence/MySqlConnection.hpp"
#include "core/Dispatcher.hpp"
#include "handler/StaticFileHandler.hpp"
#include "net/TcpServer.hpp"
#include "net/TcpConnection.hpp"
#include "persistence/ThreadLocalSqlConn.hpp"
#include "protocol/HttpRequest.hpp"
#include "protocol/HttpRouter.hpp"
#include "spdlog/spdlog.h"
#include "utils/JsonConfigLoader.hpp"


int main(int argc, const char **argv)
{
    if (argc < 3)
    {
        std::cerr
            << "Usage: " << argv[0]
            << " <port> <resource_path> [dispatcher: epoll|poll|select] [threads] [conn_mode: "
               "close|keepalive] "
               "[keepalive_max_requests] [keepalive_idle_ms]\n";
        return -1;
    }

    const auto origCwd = std::filesystem::current_path();

    const auto port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (chdir(argv[2]) != 0)
    {
        std::cerr << "资源目录切换失败: " << argv[2] << "\n";
        return -1;
    }
    
    const auto dispatcherType = (argc >= 4) ? reactor::core::dispatcherTypeFromString(
                                                  argv[3], reactor::core::DispatcherType::kEpoll)
                                            : reactor::core::DispatcherType::kEpoll;
    const auto maxThreads = (argc >= 5) ? static_cast<uint32_t>(std::atoi(argv[4])) : 4U;
    const std::string connMode = (argc >= 6) ? argv[5] : "close";
    const bool keepAliveEnabled = (connMode == "keepalive");
    const auto keepAliveMaxRequests =
        (argc >= 7) ? static_cast<uint32_t>(std::atoi(argv[6])) : 100U;
    const auto keepAliveIdleMs = (argc >= 8) ? static_cast<uint32_t>(std::atoi(argv[7])) : 10000U;
    const auto staticRoot = std::filesystem::current_path();

    spdlog::set_level(spdlog::level::debug);
    spdlog::debug("资源定向成功，开始启动服务器，dispatcher={}, threads={}, conn_mode={}, "
                  "keepalive_max_requests={}, keepalive_idle_ms={}",
                  reactor::core::dispatcherTypeToString(dispatcherType), maxThreads,
                  keepAliveEnabled ? "keepalive" : "close", keepAliveMaxRequests, keepAliveIdleMs);

    try
    {
        reactor::net::TcpServer server(port, maxThreads, dispatcherType, keepAliveEnabled,
                                       keepAliveMaxRequests, keepAliveIdleMs);

        //配置挂载目录
        auto sqlConfigPath = origCwd/"config"/"SQLConfig.json";
        auto sqlCfg = reactor::utils::config::loadJsonFileOrThrow(sqlConfigPath);
        spdlog::debug("SQL 配置加载完成 {}",sqlConfigPath.string());

        auto sqlExecutor = std::make_shared<reactor::persistence::SqlExecutor>(
                reactor::persistence::ThreadLocalSingleConn(
                reactor::persistence::ThreadLocalConnOptions{},
                [sqlCfg]()->
                std::unique_ptr<reactor::persistence::ISqlConnection>
                {
                    return std::make_unique<reactor::persistence::MySqlConnection>(sqlCfg);
                }
            )
        );
        sqlExecutor->start();

        auto sqlHandler = std::make_shared<reactor::handler::SqlHandler>(sqlExecutor);
        auto staticHandler = std::make_shared<reactor::handler::StaticFileHandler>(staticRoot);

        reactor::net::protocol::HttpRouter router;
        router.get("/query", sqlHandler);
        router.get("/sql", sqlHandler);
        router.addPrefix("/", staticHandler);

        server.setRequestHandler(
                [router = std::move(router)]
                (reactor::net::protocol::HttpRequest &req,
                 reactor::net::TcpConnection &conn) mutable
                -> reactor::net::TcpConnection::HandlerResult
                {
                    return router.dispatch(req, conn);
                }
            );
        const auto status = server.run();
        if(status != reactor::core::StatusCode::kOk)
        {
            std::cout << "服务器运行异常：" << reactor::core::toInt(status) << "\n";
            return -1;
        }
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "服务器初始化/运行异常: " << ex.what() << "\n";
        return -1;
    }
}
