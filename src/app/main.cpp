#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>
#include <unistd.h>


#include "core/CoreStatus.hpp"
#include "handler/SqlHandler.hpp"
#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlConfig.hpp"
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

namespace
{
    void printUsage(const char *program)
    {
        std::cerr
            << "Usage: " << program
            << " <port> <resource_path> [dispatcher: epoll|poll|select] [threads] [conn_mode: "
               "close|keepalive] [keepalive_max_requests] [keepalive_idle_ms] [--enable-sql]\n"
            << "\n"
            << "  --enable-sql   启用 /query 与 /sql 演示端点（默认关闭；开启后从 config/SQLConfig.json 读取连接配置）\n"
            << "  --help         显示本帮助\n";
    }
} // namespace


int main(int argc, const char **argv)
{
    bool enableSql = false;
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--enable-sql")
        {
            enableSql = true;
            continue;
        }
        if (arg == "--help" || arg == "-h")
        {
            printUsage(argv[0]);
            return 0;
        }
        args.push_back(arg);
    }

    if (args.size() < 2)
    {
        printUsage(argv[0]);
        return -1;
    }

    const auto origCwd = std::filesystem::current_path();

    const auto port = static_cast<uint16_t>(std::atoi(args[0].c_str()));
    if (chdir(args[1].c_str()) != 0)
    {
        std::cerr << "资源目录切换失败: " << args[1] << "\n";
        return -1;
    }

    const auto dispatcherType =
        (args.size() >= 3) ? reactor::core::dispatcherTypeFromString(
                                 args[2], reactor::core::DispatcherType::kEpoll)
                           : reactor::core::DispatcherType::kEpoll;
    const auto maxThreads = (args.size() >= 4) ? static_cast<uint32_t>(std::atoi(args[3].c_str()))
                                               : 4U;
    const std::string connMode = (args.size() >= 5) ? args[4] : "close";
    const bool keepAliveEnabled = (connMode == "keepalive");
    const auto keepAliveMaxRequests =
        (args.size() >= 6) ? static_cast<uint32_t>(std::atoi(args[5].c_str())) : 100U;
    const auto keepAliveIdleMs =
        (args.size() >= 7) ? static_cast<uint32_t>(std::atoi(args[6].c_str())) : 10000U;
    const auto staticRoot = std::filesystem::current_path();

    spdlog::set_level(spdlog::level::debug);
    spdlog::debug("资源定向成功，开始启动服务器，dispatcher={}, threads={}, conn_mode={}, "
                  "keepalive_max_requests={}, keepalive_idle_ms={}, enable_sql={}",
                  reactor::core::dispatcherTypeToString(dispatcherType), maxThreads,
                  keepAliveEnabled ? "keepalive" : "close", keepAliveMaxRequests, keepAliveIdleMs,
                  enableSql);

    try
    {
        reactor::net::TcpServer server(port, maxThreads, dispatcherType, keepAliveEnabled,
                                       keepAliveMaxRequests, keepAliveIdleMs);

        std::shared_ptr<reactor::handler::SqlHandler> sqlHandler;

        if (enableSql)
        {
            // SQL 端点显式开启时才读取配置；默认关闭让静态文件服务不依赖数据库。
            const auto sqlConfigPath = origCwd / "config" / "SQLConfig.json";
            const auto sqlConfig =
                reactor::utils::config::loadJsonFileOrThrow(sqlConfigPath)
                    .get<reactor::persistence::SqlConfig>();
            spdlog::debug("SQL 配置加载完成 {}", sqlConfigPath.string());

            auto executor = std::make_shared<reactor::persistence::SqlExecutor>(
                reactor::persistence::ThreadLocalSingleConn(
                    reactor::persistence::ThreadLocalConnOptions{},
                    [sqlConfig]() -> std::unique_ptr<reactor::persistence::ISqlConnection>
                    {
                        return std::make_unique<reactor::persistence::MySqlConnection>(sqlConfig);
                    }));
            executor->start();

            sqlHandler = std::make_shared<reactor::handler::SqlHandler>(executor, sqlConfig.readOnly);
            if (sqlConfig.readOnly)
            {
                spdlog::info("SQL 端点已开启（只读模式：仅允许 SELECT/SHOW/DESCRIBE/EXPLAIN）");
            }
            else
            {
                spdlog::warn("SQL 端点已开启且 readOnly=false：将允许任意 SQL，请仅在本机或隔离网络使用");
            }
        }
        else
        {
            spdlog::info("SQL 端点未启用（需要时追加 --enable-sql）");
        }

        auto staticHandler = std::make_shared<reactor::handler::StaticFileHandler>(staticRoot);

        reactor::net::protocol::HttpRouter router;
        if (sqlHandler)
        {
            router.get("/query", sqlHandler);
            router.get("/sql", sqlHandler);
        }
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
