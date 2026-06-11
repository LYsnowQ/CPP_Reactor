#pragma once

#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlConfig.hpp"
#include "persistence/SqlTypes.hpp"

#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>

#include <string_view>
#include <memory>

namespace reactor::persistence
{

/// @brief MySQL Connector/C++（JDBC）连接实现
///
/// 基于 MySQL 官方 Connector/C++（libmysqlcppconn）的 ISqlConnection 实现。
/// 所有方法均返回 Result 而非抛出异常（内部捕获 std::exception 后映射到 SqlErrc）。
///
/// @param cfg 连接配置（构造时记录，connect 时使用）
/// @thread 非线程安全，同一实例只应在单线程中使用
class MySqlConnection final : public ISqlConnection
{
public:
    /// @brief 构造 MySQL 连接（不实际连接，仅记录配置和获取 driver 实例）
    /// @param cfg 连接配置
    explicit MySqlConnection(const SqlConfig& cfg);

    /// @brief 析构时自动调用 close()
    ~MySqlConnection() override;

    /// @brief 使用 ConnectOptionsMap 建立连接
    Result<void> connect() override;

    /// @brief 执行 "DO 1" 检测连接存活
    Result<void> ping() override;

    /// @brief 查询分支：无参数时使用 Statement.executeQuery，有参数时使用 PreparedStatement
    Result<SqlRows> query(std::string_view sql, const std::vector<SqlValue>& args) override;

    /// @brief 更新分支：无参数时使用 Statement.execute，有参数时使用 PreparedStatement
    Result<uint64_t> execute(std::string_view sql, const std::vector<SqlValue>& args) override;

    /// @brief 关闭连接并 reset unique_ptr
    void close() noexcept override;

    /// @brief setAutoCommit(false)
    Result<void> begin() override;

    /// @brief commit() + setAutoCommit(true)
    Result<void> commit() override;

    /// @brief rollback() + setAutoCommit(true)
    Result<void> rollback() override;

private:
    SqlConfig cfg_;
    sql::Driver* driver_{nullptr};
    std::unique_ptr<sql::Connection> conn_;
};

}//namespace reactor::persistence
