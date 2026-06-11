#include "repository/sql/SqlRepository.hpp"
#include "persistence/ISqlConnection.hpp"
#include "persistence/SqlExecutor.hpp"
#include "persistence/SqlTypes.hpp"
#include "persistence/ThreadLocalSqlConn.hpp"
#include <cstdint>
#include <exception>

namespace reactor::repository::sql
{
    // ====================================================================
    // 构造
    // ====================================================================
    //
    // 仅保存 executor 引用，不建立任何连接或分配资源。
    // executor 的生命周期由调用方（如 main.cpp）保证。
    SqlRepository::SqlRepository(persistence::SqlExecutor &executor)
        : executor_(executor)
    {}

    // ====================================================================
    // 异步用户查询
    // ====================================================================
    //
    // 1. executor_.submit 将 lambda 提交到 SQL 后台线程。
    // 2. 后台线程中通过 tls.withConnection 获取线程局部连接并执行查询。
    // 3. 查询成功时 res 包含 SqlRows 结果集。
    // 4. 查询异常时 res 标记为 kConnection 错误。
    // 5. 无论成功或失败，均通过 done(std::move(res)) 将结果回传。
    //
    // 注意：done 在 SqlExecutor 的后台线程中执行，而非 EventLoop 线程。
    // 调用方若需操作 TcpConnection，必须通过 EventLoop::post 切回。
    bool SqlRepository::findUserByIdAsync(int64_t userId, Done done)
    {
        return executor_.submit(
            [userId, done = std::move(done)](persistence::ThreadLocalSingleConn &tls) mutable {
                persistence::Result<persistence::SqlRows> res{};
                try {
                    res = tls.withConnection(
                        [&](persistence::ISqlConnection &conn) {
                            return conn.query(
                                "SELECT id,name,status FROM users WHERE id = ?",
                                {userId});
                        });
                } catch (const std::exception &ex) {
                    res.ok = false;
                    res.err.code = persistence::SqlErrc::kConnection;
                    res.err.message = ex.what();
                }
                // 无论查询成功或失败，均通过回调返回结果给调用方。
                done(std::move(res));
            });
    }

} // namespace reactor::repository::sql
