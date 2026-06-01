#include "persistence/MySqlConnection.hpp"
#include "jdbc/cppconn/connection.h"
#include "jdbc/cppconn/datatype.h"
#include "jdbc/cppconn/prepared_statement.h"
#include "jdbc/cppconn/resultset.h"
#include "jdbc/cppconn/sqlstring.h"
#include "jdbc/cppconn/statement.h"
#include "jdbc/mysql_driver.h"
#include "persistence/SqlTypes.hpp"
#include <cstdint>
#include <exception>
#include <memory>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace reactor::persistence {

    namespace
    {
        void bindArgs(sql::PreparedStatement& stmt,const std::vector<SqlValue>& args)
        {
            for(size_t i = 0;i < args.size(); ++i)
            {
                const auto& arg = args[i];
                std::visit([&](auto&& val)
                    {
                        using T = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<T, std::nullptr_t>)
                        {
                            stmt.setNull(static_cast<int>(i+1),0);
                        }
                        else if constexpr(std::is_same_v<T,int64_t>)
                        {
                            stmt.setInt64(static_cast<int>(i+1),val);
                        }
                        else if constexpr(std::is_same_v<T, uint64_t>)
                        {
                            stmt.setUInt64(static_cast<int>(i+1),val);
                        }
                        else if constexpr(std::is_same_v<T, float>)
                        {
                            stmt.setDouble(static_cast<int>(i+1),static_cast<double>(val));
                        }
                        else if constexpr(std::is_same_v<T, double>)
                        {
                            stmt.setDouble(static_cast<int>(i+1),val);
                        }
                        else if constexpr(std::is_same_v<T, std::string>)
                        {
                            stmt.setString(static_cast<int>(i+1),val);
                        }
                    },arg);
            }
        }

        SqlRow rowToSqlRow(sql::ResultSet& rs, int colCount)
        {
            SqlRow row;
            for(int i = 1; i <= colCount; ++i)
            {
                int type = rs.getMetaData()->getColumnType(i);
                switch (type) {
                    case sql::DataType::INTEGER:
                    case sql::DataType::SMALLINT:
                    case sql::DataType::TINYINT:
                    case sql::DataType::BIGINT:
                        row.push_back(static_cast<int64_t>(rs.getInt64(i)));
                        break;
                    case sql::DataType::BIT:
                        row.push_back(static_cast<uint64_t>(rs.getUInt64(i)));
                        break;
                    case sql::DataType::REAL:
                    case sql::DataType::DOUBLE:
                    case sql::DataType::DECIMAL:
                        row.push_back(static_cast<double>(rs.getDouble(i)));
                        break;
                    case sql::DataType::VARCHAR:
                    case sql::DataType::CHAR:   
                    case sql::DataType::LONGVARCHAR:
                    case sql::DataType::DATE:
                    case sql::DataType::TIMESTAMP:
                    case sql::DataType::TIME:
                        row.push_back(rs.getString(i));
                        break;
                    default:
                        if(rs.isNull(i))
                            row.push_back(nullptr);
                        else
                            row.push_back(rs.getString(i));
                }
            }
            return row;
        }
    }//namespace


    MySqlConnection::MySqlConnection(const SqlConfig& cfg)
    :cfg_(cfg)
    {
        driver_ = sql::mysql::get_driver_instance();
    }


    MySqlConnection::~MySqlConnection()
    {
        close();
    }


    Result<void> MySqlConnection::connect()
    {
        Result<void> res{};
        try
        {
            sql::ConnectOptionsMap opts;
            opts["hostName"] = cfg_.host;
            opts["port"] = static_cast<int>(cfg_.port);
            opts["userName"] = cfg_.user;
            opts["password"] = cfg_.password;
            if(!cfg_.database.empty())
            {
                opts["schema"] = cfg_.database;
            }
            opts["OPT_RECONNECT"] = true;
            opts["OPT_CONNECT_TIMEOUT"] = static_cast<int>(cfg_.connectTimeoutMs/1000);
            opts["OPT_READ_TIMEOUT"] = static_cast<int>(cfg_.readTimeoutMs/1000);
            opts["OPT_WRITE_TIMEOUT"] = static_cast<int>(cfg_.writeTimeoutMs/1000);

            if(!cfg_.charset.empty())
            {
                opts["OPT_CHARSET_NAME"] = cfg_.charset;
                opts["characterSetResults"] = cfg_.charset;
            }

            auto* rawConn = driver_->connect(opts);
            if(!rawConn)
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "driver returned null connection";
                return res;
            }
            conn_.reset(rawConn);

            /*
            std::ostringstream url;
            url << "tcp://" << cfg_.host << ":" << cfg_.port;
            std::string urlStr = url.str();

            sql::SQLString connectTimeout(std::to_string(cfg_.connectTimeoutMs/1000)); 
            sql::SQLString readTimeoutMs(std::to_string(cfg_.readTimeoutMs/1000));

            conn_.reset(driver_->connect(urlStr, cfg_.user,cfg_.password));
            if(!cfg_.database.empty())
            {
                conn_->setSchema(cfg_.database);
            }
            */

            conn_->setAutoCommit(true);
            res.ok = true;
        }
        catch(const std::exception& err)
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
            if(!conn_ || conn_->isClosed())
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "no Connection";
                return res;
            }
            std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
            stmt->execute("DO 1");
            res.ok = true;
        }
        catch(const std::exception&)
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
            if(!conn_ || conn_->isClosed())
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "not connected";
                return res;
            }

            if(args.empty())
            {
                std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
                if(!stmt)
                {
                    res.ok = false;
                    res.err.code = SqlErrc::kQuery;
                    res.err.message = "createStatement faild";
                    return res;
                }
                std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery(std::string(sql)));
            
                int colCount = rs->getMetaData()->getColumnCount();
                SqlRows rows;
                while(rs->next())
                {
                    rows.push_back(rowToSqlRow(*rs,colCount));
                }
                res.ok = true;
                res.value = std::move(rows);
            }
            else
            {
                std::unique_ptr<sql::PreparedStatement> pstmt(conn_->prepareStatement(std::string(sql)));
                if(!pstmt)
                {
                    res.ok = false;
                    res.err.code = SqlErrc::kQuery;
                    res.err.message = "createStatement faild";
                    return res;
                }
                bindArgs(*pstmt, args);

                std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());

                int colCount = rs->getMetaData()->getColumnCount();
                SqlRows rows;
                while(rs->next())
                {
                    rows.push_back(rowToSqlRow(*rs, colCount));
                }
                res.ok = true;
                res.value = std::move(rows);
            }
        }
        catch(const std::exception& err)
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
            if(!conn_ || conn_->isClosed())
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "no connected";
                return res;
            }

            if(args.empty())
            {
                std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
                if(!stmt)
                {
                    res.ok = false;
                    res.err.code = SqlErrc::kQuery;
                    res.err.message = "createStatement faild";
                    return res;
                }
                stmt->execute(std::string(sql));
                res.ok = true;
                res.value = stmt->getUpdateCount();    
            }
            else
            {
                std::unique_ptr<sql::PreparedStatement> pstmt(conn_->prepareStatement(std::string(sql)));
                if(!pstmt)
                {
                    res.ok = false;
                    res.err.code = SqlErrc::kQuery;
                    res.err.message = "createStatement faild";
                    return res;
                }
                bindArgs(*pstmt, args);
                pstmt->execute();
                res.ok = true;
                res.value = pstmt->getUpdateCount();
            }
        }
        catch(const std::exception& err)
        {
            res.ok = false;
            res.err.code = SqlErrc::kConnection;
            res.err.message = err.what();
        }
        return res;
    }


    void MySqlConnection::close() noexcept
    {
        try
        {
            if(conn_)
            {
                conn_->close();
                conn_.reset();
            }
        }
        catch(...)
        {}
    }

    Result<void> MySqlConnection::begin()
    {
        Result<void> res;
        try
        {
            if(!conn_ || conn_->isClosed())
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "no connected";
                return res;
            }

            conn_ ->setAutoCommit(false);
            res.ok = true;
        }
        catch(const std::exception& err)
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
            if(!conn_|| conn_->isClosed())
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "no connected";
                return res;
            }

            conn_->commit();
            conn_->setAutoCommit(true);
            res.ok = true;
        }
        catch(const std::exception& err)
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
            if(!conn_ || conn_->isClosed())
            {
                res.ok = false;
                res.err.code = SqlErrc::kConnection;
                res.err.message = "no connected";
                return res;
            }

            conn_->rollback();
            conn_->setAutoCommit(true);
            res.ok = true;
        }
        catch(const std::exception& err)
        {
            res.ok = false;
            res.err.code = SqlErrc::kConnection;
            res.err.message = err.what();
        }
        return res;


    }
}//namespace reactor::persistence
