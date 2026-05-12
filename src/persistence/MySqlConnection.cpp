#include "persistence/MySqlConnection.hpp"
#include "persistence/SqlTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mysqlx/devapi/common.h>
#include <mysqlx/devapi/document.h>
#include <mysqlx/devapi/result.h>
#include <mysqlx/xdevapi.h>
#include <utility>
#include <variant>
#include <vector>

namespace reactor::persistence {

    namespace
    {
        void bindArgs(mysqlx::SqlStatement& stmt,const std::vector<SqlValue>& args)
        {
            for(const auto& arg : args )
            {
                std::visit([&stmt](auto&& val)
                    {
                        using T = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<T, std::nullptr_t>)
                        {
                            stmt.bind(nullptr);
                        }
                        else if constexpr(std::is_same_v<T,int64_t>)
                        {
                            stmt.bind(static_cast<int64_t>(val));
                        }
                        else if constexpr(std::is_same_v<T, uint64_t>)
                        {
                            stmt.bind(static_cast<uint64_t>(val));
                        }
                        else if constexpr(std::is_same_v<T, float>)
                        {
                            stmt.bind(val);
                        }
                        else if constexpr(std::is_same_v<T, double>)
                        {
                            stmt.bind(val);
                        }
                        else if constexpr(std::is_same_v<T, std::string>)
                        {
                            stmt.bind(val);
                        }
                    },arg);
            }
        }
    }//namespace


    MySqlConnection::MySqlConnection(const SqlConfig& cfg)
    :cfg_(cfg)
    {}


    MySqlConnection::~MySqlConnection()
    {
        close();
    }


    Result<void> MySqlConnection::connect()
    {
        Result<void> res{};
        try
        {
            Session_ = std::make_unique<mysqlx::Session>(cfg_.host,cfg_.port,cfg_.user,cfg_.password);
            if(!cfg_.database.empty())
            {
                Session_->sql("USE "+ cfg_.database).execute();
            }
            res.ok = true;
        }
        catch(const mysqlx::Error& err)
        {
            res.ok = false;
            res.err.code = SqlErrc::kConnection;
            res.err.message = err.what();
        }
        return res;
    }

    Result<void> MySqlConnection::ping()
    {
        Result<void> res;
        try
        {
            if(!Session_)
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "no Connection";
                return res;
            }
            Session_->sql("DO 1").execute();
            res.ok = true;
        }
        catch(const mysqlx::Error& err)
        {
            res.ok = false;
            res.err.code = SqlErrc::kConnection;
        }
        return res;
    }


    Result<SqlRows> MySqlConnection::query(std::string_view sql,const std::vector<SqlValue>& args)
    {
        Result<SqlRows> res;
        try
        {
            if(!Session_)
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "not connected";
                return res;
            }

            auto stmt = Session_->sql(std::string(sql));
            bindArgs(stmt,args);
            auto result = stmt.execute();

            SqlRows rows;
            for(mysqlx::Row row: result)
            {
                SqlRow sqlRow;
                for(unsigned long i = 0;i < row.colCount(); i++)
                {
                    SqlValue cell;

                    switch (row[i].getType())
                    {
                        case mysqlx::Value::Type::INT64:
                            cell = static_cast<int64_t>(row[i].get<int64_t>());
                            break;
                        case mysqlx::Value::Type::UINT64:
                            cell = static_cast<uint64_t>(row[i].get<uint64_t>());
                            break;
                        case mysqlx::Value::Type::STRING:
                            cell = static_cast<std::string>(row[i].get<std::string>());
                            break;
                        case mysqlx::Value::Type::FLOAT:
                        case mysqlx::Value::Type::DOUBLE:
                            cell = static_cast<double>(row[i].get<double>());
                            break;
                        default:
                            cell = nullptr;
                    }
                    sqlRow.push_back(std::move(cell));
                }
                rows.push_back(std::move(sqlRow));
            }
            res.ok = true;
            res.value = std::move(rows);
        }
        catch(const mysqlx::Error& err)
        {
            res.ok =false;
            res.err.code = SqlErrc::kQuery;
            res.err.message = err.what();
        }

    return res;
    }


    Result<uint64_t> MySqlConnection::execute(std::string_view sql,const std::vector<SqlValue>& args)
    {
        Result<uint64_t> res;
        try
        {
            if(!Session_)
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "no connected";
                return res;
            }

            auto stmt = Session_->sql(std::string(sql));
            bindArgs(stmt, args);

            auto result = stmt.execute();
            res.ok = true;
            res.value = result.getAffectedItemsCount();

        }
        catch(const mysqlx::Error& err)
        {
            res.ok = false;
            res.err.code = SqlErrc::kConnection;
            res.err.message = err.what();
        }
        return res;
    }


    void MySqlConnection::close() noexcept
    {
        if(Session_)
        {
            Session_->close();
        }
    }

    Result<void> MySqlConnection::begin()
    {
        Result<void> res;
        try
        {
            if(!Session_)
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "no connected";
                return res;
            }

            Session_->sql("START TRANSACTION").execute();
            res.ok = true;
        }
        catch(const mysqlx::Error& err)
        {
            res.ok = false;
            res.err.code = SqlErrc::kConnection;
            res.err.message = err.what();
        }
        return res;

    }


    Result<void> MySqlConnection::commit()
    {
        Result<void> res;
        try
        {
            if(!Session_)
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "no connected";
                return res;
            }

            Session_->sql("COMMIT").execute();
            res.ok = true;
        }
        catch(const mysqlx::Error& err)
        {
            res.ok = false;
            res.err.code = SqlErrc::kConnection;
            res.err.message = err.what();
        }
        return res;


    }


    Result<void> MySqlConnection::rollback()
    {
        Result<void> res;
        try
        {
            if(!Session_)
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "no connected";
                return res;
            }

            Session_->sql("ROLLBACK").execute();
            res.ok = true;
        }
        catch(const mysqlx::Error& err)
        {
            res.ok = false;
            res.err.code = SqlErrc::kConnection;
            res.err.message = err.what();
        }
        return res;


    }
}//namespace reactor::persistence
