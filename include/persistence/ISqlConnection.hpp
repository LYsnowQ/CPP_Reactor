#pragma once

#include "persistence/SqlTypes.hpp"

#include <string_view>
#include <vector>

namespace reactor::persistence
{

/// @brief SQL 连接抽象接口
///
/// 定义 SQL 数据库连接的标准操作集合，所有具体数据库实现（如 MySqlConnection）
/// 需继承此类并实现全部纯虚方法。
///
/// @thread 非线程安全，单连接实例应只在一个线程中使用
class ISqlConnection
{
  public:
    virtual ~ISqlConnection() = default;

    /// @brief 建立数据库连接
    /// @retval res.ok = true  连接成功
    /// @retval res.ok = false 连接失败（res.err.code = kConnection）
    virtual Result<void> connect() = 0;

    /// @brief 检测连接是否存活
    /// @retval res.ok = true  连接正常
    /// @retval res.ok = false 连接异常（res.err.code = kConnection）
    virtual Result<void> ping() = 0;

    /// @brief 执行 SQL 查询并返回结果集
    /// @param sql  SQL 语句（支持 ? 占位符，由 args 绑定）
    /// @param args 参数列表（替换 SQL 中的 ? 占位符）
    /// @return 查询结果
    /// @retval res.ok = true  查询成功，res.value 包含结果行
    /// @retval res.ok = false 查询失败（res.err.code = kQuery/kConnection）
    virtual Result<SqlRows> query(std::string_view sql, const std::vector<SqlValue> &args) = 0;

    /// @brief 执行 SQL 更新（INSERT/UPDATE/DELETE）
    /// @param sql  SQL 语句（支持 ? 占位符）
    /// @param args 参数列表
    /// @return 执行结果
    /// @retval res.ok = true  执行成功，res.value 为受影响行数
    /// @retval res.ok = false 执行失败
    virtual Result<uint64_t> execute(std::string_view sql, const std::vector<SqlValue> &args) = 0;

    /// @brief 关闭连接
    /// @warning noexcept，不得抛出异常
    virtual void close() noexcept = 0;

    /// @brief 开启事务（关闭 auto_commit）
    virtual Result<void> begin() = 0;
    /// @brief 提交事务（恢复 auto_commit）
    virtual Result<void> commit() = 0;
    /// @brief 回滚事务（恢复 auto_commit）
    virtual Result<void> rollback() = 0;
};

} // namespace reactor::persistence
