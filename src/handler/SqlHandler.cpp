#include "handler/SqlHandler.hpp"
#include "net/TcpConnection.hpp"
#include "persistence/ThreadLocalSqlConn.hpp"
#include "protocol/HttpRequest.hpp"
#include "utils/StringUtils.hpp"
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
    }//namespace


    net::TcpConnection::RequestHandler SqlHandler::createHandler(std::shared_ptr<persistence::SqlExecutor> sqlExecutor)
    {
        return [sqlExecutor](
                net::protocol::HttpRequest & req,
                net::TcpConnection &conn)
            ->net::TcpConnection::HandlerResult
        {
            net::TcpConnection::HandlerResult result;
            result.contentType = "application/json; charset=utf-8";

            //暂时只放行get请求
            if(req.getMethed() != "GET")
            {
                result.statusCode = 405;
                result.body = R"({"error":"Method not allowd"})";
                return result;
            }

            auto sql = extractSqlParam_(req.getUrl());
            if(sql.empty())
            {
                result.statusCode = 400;
                result.body = R"({"error":"Missing 'sql' query parameter"})";
                return result;
            }

            result.async = true;

            core::EventLoop *loop = conn.getLoop();
            std::weak_ptr<net::TcpConnection> weakConn = conn.shared_from_this();
        
            sqlExecutor->submit(
                    [sql = std::move(sql),loop,weakConn]
                    (persistence::ThreadLocalSingleConn &tls)
                    {
                        auto sqlResult = tls.withConnection(
                                [&sql]
                                (persistence::ISqlConnection &db)
                                {
                                    return db.query(sql, {});
                                }
                            );
                        loop->post(
                                [weakConn,sqlResult = std::move(sqlResult)]
                                ()
                                {
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
                );
            return result;
        }; 
    }
}//namespace reactor::handler
