#include "persistence/SqlTransaction.hpp"
#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlTypes.hpp"


namespace reactor::persistence
{
    // ====================================================================
    // 构造：开启事务
    // ====================================================================
    //
    // 构造时立即调用 conn.begin()，结果存入 begin_。
    // valid() 返回 begin_.ok，调用方应检查是否成功开启事务。
    SqlTransaction::SqlTransaction(ISqlConnection& conn)
        :conn_(conn)
    {
        begin_ = conn_.begin();
    }

    // ====================================================================
    // 析构：自动回滚
    // ====================================================================
    //
    // 仅当满足以下所有条件时自动回滚：
    //   1. finished_ == false（未手动 commit/rollback）
    //   2. begin_.ok == true（事务已成功开启）
    // 析构中不应抛出异常，回滚失败时静默处理。
    SqlTransaction::~SqlTransaction()
    {
        if(!finished_ && begin_.ok)
        {
            conn_.rollback();
        }
    }

    bool SqlTransaction::valid() const
    {
        return begin_.ok;
    }

    Result<void> SqlTransaction::commit()
    {
        auto res = conn_.commit();
        if(res.ok)
        {
            finished_ = true;
        }
        return res;
    }

    Result<void> SqlTransaction::rollback()
    {
        finished_ = true;
        return conn_.rollback();
    }

    Result<SqlRows> SqlTransaction::query(std::string_view sql, const std::vector<SqlValue>& args)
    {
        return conn_.query(sql, args);
    }

    Result<uint64_t> SqlTransaction::execute(std::string_view sql, const std::vector<SqlValue> &args)
    {
        return conn_.execute(sql, args);
    }

}//namespace reactor::persistence
