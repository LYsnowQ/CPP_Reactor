#include "persistence/SqlTransaction.hpp"
#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlTypes.hpp"


namespace reactor::persistence
{
    SqlTransaction::SqlTransaction(ISqlConnection& conn)
        :conn_(conn)
    {
        begin_ = conn_.begin();
    }

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

    Result<SqlRows> SqlTransaction::query(std::string_view sql,const std::vector<SqlValue>& args)
    {
        return conn_.query(sql, args);
    }

    Result<uint64_t> SqlTransaction::execute(std::string_view sql, const std::vector<SqlValue> &args)
    {
        return conn_.execute(sql, args);
    }

}//namespace reactor::persistence
