#include "handler/SqlHandler.hpp"
#include "net/TcpConnection.hpp"
#include "persistence/ThreadLocalSqlConn.hpp"
#include "protocol/HttpRequest.hpp"
#include "utils/StringUtils.hpp"
#include "utils/SqlStatementGuard.hpp"
#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlTypes.hpp"
#include "core/EventLoop.hpp"
#include <cmath>
#include <nlohmann/json.hpp>
#include <type_traits>
#include <variant>
#include <sstream>
#include <memory>
#include <string_view>

namespace reactor::handler
{
    namespace
    {
        // ====================================================================
        // URL 参数提取
        // ====================================================================
        //
        // 从 URL 的 query string 中提取 "sql" 参数的值。
        // 支持多参数场景（如 ?sql=SELECT 1&format=json），
        // 仅提取名为 "sql" 的参数，其他参数忽略。
        // 返回值经过 urlDecode 解码（处理 %XX 编码）。
        std::string extractSqlParam_(std::string_view url)
        {
            auto q = url.find('?');
            if(q == std::string::npos)
            {
                return "";
            }

            auto qs = url.substr(q+1);
            size_t start = 0;
            while(start < qs.size())
            {
                auto amp = qs.find('&',start);
                auto seg = qs.substr(start,amp-start);
                auto eq = seg.find('=');
                if(eq!=std::string::npos)
                {
                    auto key = seg.substr(0,eq);
                    if(key == "sql")
                    {
                        return reactor::utils::urlDecode(seg.substr(eq+1));
                    }
                }

                if(amp == std::string::npos)
                {
                    break;
                }
                start = amp + 1;
            }
            return "";
        }

        // ====================================================================
        // SqlRows → JSON 格式化
        // ====================================================================
        //
        // 将 SqlExecutor 返回的二维行数据转换为 JSON 数组。
        // 每行为一个 JSON 数组，每个单元格使用 std::visit 按 variant 类型分发：
        //   nullptr_t → null
        //   uint64_t/int64_t → 数字
        //   double/float → 数字
        //   string → 字符串
        // 输出格式为美化排版（缩进 2 空格）。
        std::string formatRowsToJson_(const reactor::persistence::SqlRows &rows)
        {
            nlohmann::json j = nlohmann::json::array();
            for(const auto &row : rows)
            {
                nlohmann::json jRow = nlohmann::json::array();
                for(const auto &cell : row)
                {
                    std::visit([&](auto &&val)
                            {
                                using T = std::decay_t<decltype(val)>;
                                if constexpr (std::is_same_v<T, std::nullptr_t>)
                                {    
                                    jRow.push_back(nullptr);
                                }
                                else if constexpr(std::is_same_v<T, uint64_t>)
                                {
                                    jRow.push_back(val);
                                }
                                else if constexpr(std::is_same_v<T, int64_t>)
                                {
                                    jRow.push_back(val);
                                }
                                else if constexpr(std::is_same_v<T, double>)
                                {
                                    jRow.push_back(val);
                                }
                                else if constexpr(std::is_same_v<T, float>)
                                {
                                    jRow.push_back(val);
                                }
                                else if constexpr(std::is_same_v<T, std::string>)
                                {
                                    jRow.push_back(val);
                                }
                            },cell);
                }
                j.push_back(std::move(jRow));
            }
            return j.dump(2);
        }

        // 异步失败也必须回包：连接失败、ping 失败等异常在 SqlExecutor
        // 工作线程被捕获后，通过 EventLoop::post 回到连接线程写 500。
        void postSqlError_(core::EventLoop *loop,
                           std::weak_ptr<net::TcpConnection> weakConn,
                           std::string message)
        {
            loop->post([weakConn, message = std::move(message)]()
            {
                auto conn = weakConn.lock();
                if(!conn)
                {
                    return;
                }

                net::TcpConnection::HandlerResult resp;
                resp.contentType = "application/json; charset=utf-8";
                resp.statusCode = 500;
                nlohmann::json err;
                err["error"] = "SQL query failed";
                err["message"] = message;
                resp.body = err.dump();
                conn->sendAsyncResponse(resp);
            });
        }
    }//namespace


    SqlHandler::SqlHandler(std::shared_ptr<persistence::SqlExecutor> executor, bool readOnly)
        : executor_(std::move(executor)), readOnly_(readOnly)
    {}

    // ====================================================================
    // SQL 查询处理器主流程（异步模式）
    // ====================================================================
    //
    // 1. 方法校验：仅放行 GET，其余返回 405（同步返回）。
    // 2. 提取 SQL：从 URL 提取 ?sql= 参数，缺失返回 400。
    // 3. 只读护栏：readOnly 模式下拒绝非只读语句（403）。
    // 4. 异步标记：result.async = true，告知 TcpConnection 不立即发送响应。
    // 5. 提交执行：executor_->submit 将 lambda 投递到 SQL 后台线程。
    // 6. 后台线程中：
    //    a. tls.withConnection 获取/创建线程局部数据库连接。
    //    b. db.query(sql, {}) 执行 SQL 查询。
    // 7. 结果回写：
    //    a. loop->post 将 lambda 投递回 EventLoop 线程。
    //    b. conn.sendAsyncResponse 在 EventLoop 线程中触发写事件。
    //    c. 若连接已断开（weak_ptr.lock() 返回 null），直接返回。
    net::TcpConnection::HandlerResult SqlHandler::handle(
        net::protocol::HttpRequest & req,
        net::TcpConnection &conn)
    {
        net::TcpConnection::HandlerResult result;
        result.contentType = "application/json; charset=utf-8";

        // 暂时只放行 GET 请求。
        if(req.getMethod() != "GET")
        {
            result.statusCode = 405;
            result.body = R"({"error":"Method not allowed"})";
            return result;
        }

        auto sql = extractSqlParam_(req.getUrl());
        if(sql.empty())
        {
            result.statusCode = 400;
            result.body = R"({"error":"Missing 'sql' query parameter"})";
            return result;
        }

        // 只读模式只放行单条 SELECT/SHOW/DESCRIBE/EXPLAIN。
        // 这是 HTTP 边界的启发式护栏，不是完整 SQL 解析器；
        // 真正的安全边界仍是只读数据库账号与网络隔离。
        if(readOnly_ && !reactor::utils::isReadOnlyStatement(sql))
        {
            result.statusCode = 403;
            result.body = R"({"error":"Only read-only statements are allowed"})";
            return result;
        }

        // async = true 标记告诉 TcpConnection 不要立即发送响应，
        // 响应将在 SqlExecutor 完成查询后通过 sendAsyncResponse 发送。
        result.async = true;

        core::EventLoop *loop = conn.getLoop();
        // 使用 weak_ptr 防止连接在 SQL 查询完成前已断开。
        // lock() 返回 null 时说明连接已销毁，跳过回写。
        std::weak_ptr<net::TcpConnection> weakConn = conn.shared_from_this();

        const bool submitted = executor_->submit(
                [sql = std::move(sql),loop,weakConn]
                (persistence::ThreadLocalSingleConn &tls)
                {
                    try
                    {
                        auto sqlResult = tls.withConnection(
                                [&sql]
                                (persistence::ISqlConnection &db)
                                {
                                    return db.query(sql, {});
                                }
                            );
                        // 通过 EventLoop::post 将结果回写到正确的线程。
                        // SqlExecutor 的工作线程不能直接操作 TcpConnection 或 Channel。
                        loop->post(
                                [weakConn,sqlResult = std::move(sqlResult)]
                                ()
                                {
                                    // 连接已断开，跳过响应回写。
                                    auto conn = weakConn.lock();
                                    if(!conn)
                                    {
                                        return;
                                    }

                                    net::TcpConnection::HandlerResult resp;
                                    resp.contentType = "application/json; charset=utf-8";
                                    if(sqlResult.ok)
                                    {
                                        resp.statusCode = 200;
                                        resp.body = formatRowsToJson_(sqlResult.value.value());
                                    }
                                    else
                                    {
                                        resp.statusCode = 500;
                                        nlohmann::json err;
                                        err["error"] = "SQL query failed";
                                        err["message"] = sqlResult.err.message;
                                        resp.body = err.dump();
                                    }

                                    conn->sendAsyncResponse(resp);
                                }

                        );
                    }
                    catch(const std::exception &err)
                    {
                        // ensureConnected_ / ping 抛出的异常在这里闭环，
                        // 不能让连接停在 asyncPending_ 状态。
                        postSqlError_(loop, weakConn, err.what());
                    }
                    catch(...)
                    {
                        postSqlError_(loop, weakConn, "unknown SQL execution error");
                    }
                }
            );

        if(!submitted)
        {
            // 执行器已停止或拒绝任务：退回同步 503，不能让请求挂起。
            result.async = false;
            result.statusCode = 503;
            result.body = R"({"error":"SQL executor unavailable"})";
        }
        return result;
    }
}//namespace reactor::handler
