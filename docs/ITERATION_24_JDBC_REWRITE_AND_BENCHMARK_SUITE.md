# Iteration 24 - JDBC 连接器重构与测试压测体系完善

## 本轮目标
- 将 `MySqlConnection` 从 X DevAPI 驱动切换到 JDBC Connector/C++（`libmysqlcppconn`），兼容 MySQL 5.7 及更低版本。
- 将 SQL 模块完整接入主入口并验证全链路通断。
- 构建完整的测试与压测体系，产出基准报告。

## 技术点与代码位置

### 1. MySqlConnection JDBC 重构
- [MySqlConnection.hpp](file:///project/CPPReactor/include/persistence/MySqlConnection.hpp#L1-L40)：头文件从 `mysqlx/xdevapi.h` 切换为 `jdbc/mysql_driver.h` / `jdbc/cppconn/*` 系列；成员从 `mysqlx::Session` 改为 `sql::Driver*` + `sql::Connection`。
- [MySqlConnection.cpp](file:///project/CPPReactor/src/persistence/MySqlConnection.cpp#L1-L420)：完整 JDBC 实现：
  - **连接方式**：`ConnectOptionsMap` 传参（支持 `OPT_RECONNECT`、超时、字符集等全部配置参数）
  - **bindArgs**：`PreparedStatement::setXxx` 泛型绑定
  - **rowToSqlRow**：`ResultSet::getMetaData` 按列类型分发取值
  - **query/execute**：同时支持无参数 `Statement` 与有参数 `PreparedStatement` 两种路径
  - **begin/commit/rollback**：事务接口通过 `setAutoCommit(false)` 控制
- 注意：原 X DevAPI 实现仍保留为注释块，位于 `connect()` 方法内

### 2. Makefile 依赖修正
- [Makefile](file:///project/CPPReactor/Makefile#L8)：链接库从 `-lmysqlcppconnx` 改为 `-lmysqlcppconn`（对应 JDBC Connector 库名）

### 3. SqlExecutor 异常日志
- [SqlExecutor.cpp](file:///project/CPPReactor/src/persistence/SqlExecutor.cpp#L79-L81)：捕获 `std::exception` 并输出 `spdlog::warn`（之前仅捕获 `mysqlx::Error`）

### 4. 主入口全链路接线
- [main.cpp](file:///project/CPPReactor/src/app/main.cpp#L60-L120)：加载 `SQLConfig.json` → 创建 `SqlExecutor` → `start()` → 复合路由闭包（`/query`/`/sql` → SqlHandler，其余 → StaticFileHandler）

### 5. 压测体系
- [bench_sql.sh](file:///project/CPPReactor/tests/bench_sql.sh#L1-L177)：SQL 压测脚本，支持 S/M/L/ALL 四档位，自动端口检测、服务器启停、Markdown 结果输出
- [BENCHMARK_REPORT.md](file:///project/CPPReactor/tests/BENCHMARK_REPORT.md#L1-L114)：基准压测报告，包含环境信息（硬件/软件/远程数据库）、静态压测结果、SQL 压测结果与第三方连接测试结论
- [bench_profile_M_latest.md](file:///project/CPPReactor/tests/benchmarks/bench_profile_M_latest.md#L1-L6)：静态文件 M 档压测结果（最高 QPS 10,143）
- [bench_sql_S_latest.md](file:///project/CPPReactor/tests/benchmarks/bench_sql_S_latest.md#L1-L7)：SQL 查询 S 档压测结果（远程 MySQL，QPS ~15）

### 6. 测试基础设施
- [tests/README.md](file:///project/CPPReactor/tests/README.md#L1-L100)：完整的测试目录结构与执行说明文档
- [setup.sh](file:///project/CPPReactor/tests/setup.sh#L1-L49)：Python 虚拟环境一键初始化脚本
- [requirements.txt](file:///project/CPPReactor/tests/requirements.txt#L1-L3)：Python 依赖声明（pymysql）
- [test_smoke.sh](file:///project/CPPReactor/tests/test_smoke.sh)：从 `scripts/` 迁移至 `tests/`
- [seed_data.py](file:///project/CPPReactor/tests/seed_data.py#L1-L40)：从 `scripts/` 迁移至 `tests/`

### 7. 配置与忽略更新
- [SQLConfig.json](file:///project/CPPReactor/config/SQLConfig.json)：连接参数更新为远程腾讯云 MySQL 5.7 实例
- [.gitignore](file:///project/CPPReactor/.gitignore#L1-L28)：新增忽略规则：`tests/bench_reports/*`、`tests/benchmarks/*`、`tests/.venv/`，保留基准报告文件

### 8. 剩余 TODO 桩
| 位置 | 内容 |
|---|---|
| [HttpRouter.hpp](file:///project/CPPReactor/include/protocol/HttpRouter.hpp#L5) | 路由分发器待实现 |
| [IHttpHandler.hpp](file:///project/CPPReactor/include/handler/IHttpHandler.hpp#L5) | 请求处理器接口待实现 |
| [SqlRepository.cpp](file:///project/CPPReactor/src/repository/sql/SqlRepository.cpp#L39) | `findUserByIdAsync` 回调未接线 |

## 压测结果摘要

### 静态文件（M 档，close 模式，epoll 6线程）
| 指标 | 值 |
|------|-----|
| 最高 QPS | 10,143 |
| 最低平均延迟 | 15.83ms |
| 最大传输速率 | 11.07MB/s |

### SQL 远程查询（S 档，远程腾讯云 MySQL 5.7 单核）
| 用例 | QPS | 平均延迟 |
|------|----:|--------:|
| `SELECT count(*)` | 15.25 | 324ms |
| `SELECT ... WHERE name=` | 13.58 | 364ms |
| `SELECT ... LIMIT 100` | 17.45 | 562ms |

> SQL 压测瓶颈在远程数据库网络往返（~300-800ms），非服务器自身。

## 相比上一版本解决的问题
- **驱动兼容性**：X DevAPI → JDBC Connector，MySQL 5.7 全面兼容
- **全链路闭环**：从 HTTP → 路由 → SqlExecutor → JDBC 查询 → JSON 回写，完整可运行
- **测试体系完备**：冒烟/边界/压测三层次，Linux/Mac 双平台可用
- **压测数据可复现**：自动化脚本 + Markdown 输出 + 基准报告，便于后续回归

## 阶段版本建议

建议将当前里程碑标记为 **0.15.0**，以体现 SQL 集成阶段的完整交付：
- C 到 C++ 迁移（已完成）
- Reactor 核心稳定（已完成）
- HTTP 协议解析（已完成）
- SQL 异步查询链路（已完成）
- 测试与基准体系（已完成）
- 路由分发与仓储层（待继续）
