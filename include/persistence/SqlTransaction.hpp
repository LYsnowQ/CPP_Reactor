#pragma once
#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlTypes.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace reactor::persistence
{

/// @brief SQL 事务 RAII 包装
///
/// 构造时自动调用 conn.begin() 开启事务。
/// 析构时若事务未提交（finished_=false）且 begin 成功，自动回滚。
///
/// 不可拷贝，可移动（移动后原对象放弃所有权）。
///
/// @param conn 数据库连接（引用，调用方保证生命周期）
/// @thread 非线程安全（引用同一 ISqlConnection，需单线程使用）
class SqlTransaction
{
public:
    /// @brief 开启事务
    /// @param conn 数据库连接
    explicit SqlTransaction(ISqlConnection& conn);

    /// @brief 若事务未提交且 begin 成功，自动回滚
    ~SqlTransaction();

    SqlTransaction(const SqlTransaction&) = delete;
    SqlTransaction& operator=(const SqlTransaction&) = delete;
    SqlTransaction(SqlTransaction&&) = default;
    SqlTransaction& operator=(SqlTransaction&&) = delete;

    /// @brief 事务是否有效（begin 操作是否成功）
    bool valid() const;

    /// @brief 提交事务
    /// @retval res.ok = true  提交成功
    /// @retval res.ok = false 提交失败
    Result<void> commit();

    /// @brief 回滚事务（标记 finished_=true 禁掉析构自动回滚）
    Result<void> rollback();

    /// @brief 在事务中执行查询
    Result<SqlRows> query(std::string_view sql, const std::vector<SqlValue>& args);

    /// @brief 在事务中执行更新
    Result<uint64_t> execute(std::string_view sql, const std::vector<SqlValue>& args);

    private:
    ISqlConnection& conn_;
    Result<void> begin_;
    bool finished_{false};
};

}//namespace reactor::persistence
