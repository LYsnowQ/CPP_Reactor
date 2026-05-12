#include "repository/sql/SqlRepository.hpp"
#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlExecutor.hpp"
#include "persistence/SqlTypes.hpp"
#include "persistence/ThreadLocalSqlConn.hpp"
#include <cstdint>
#include <exception>



namespace reactor::repository::sql
{
    SqlRepository::SqlRepository(persistence::SqlExecutor& executor)
        :executor_(executor)
    {}

    bool SqlRepository::findUserByIdAsync(int64_t userId, Done done)
    {
        return executor_.submit
            (
                [userId,done = std::move(done)](persistence::ThreadLocalSingleConn& tls) mutable
                {
                    persistence::Result<persistence::SqlRows> res{};
                    try{
                        res = tls.withConnection(
                                [&](persistence::ISqlConnection& conn)
                                {
                                    return conn.query(
                                            "SELECT id,name,status FROM users WHERE id = ?"
                                            ,{userId}
                                            );
                                });
                    }catch(const std::exception& ex)
                    {
                        res.ok = false;
                        res.err.code = persistence::SqlErrc::kConnection;
                        res.err.message = ex.what();
                    }
                    //TODO:执行回调
                }
            );
    }

}//namespace reactor::repository::sql
