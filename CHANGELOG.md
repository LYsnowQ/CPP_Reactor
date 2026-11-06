# Changelog

本文件记录面向使用者的变更。格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [SemVer](https://semver.org/lang/zh-CN/)。规则见
[docs/conventions/VERSIONING.md](./docs/conventions/VERSIONING.md)。

## [Unreleased]

### Security

- `/sql` 与 `/query` 默认不注册，必须显式追加 `--enable-sql` 才开启（#1）。
- 静态文件服务拒绝根目录下的 `config/`、`.git/` 与任意点文件，目录索引也不再列出它们（#2）。
- `readOnly: true` 时启用 HTTP 层只读护栏：仅允许单条 `SELECT` / `SHOW` / `DESCRIBE` /
  `EXPLAIN`，写语句、多语句、注释开头一律 403；`readOnly: false` 显式允许任意 SQL 并输出警告。

### Added

- 新增 `docs/conventions/`：项目定位、文档、版本、Git 工作流、测试、代码审查六份规范。
- 新增 `docs/architecture/`：架构、配置、HTTP 接口三份现行事实文档。
- 新增 `docs/decisions/`：Reactor 模型、第三方库自包含、`/sql` 演示端点三条 ADR。
- 新增 `docs/plan/BACKLOG.md`：12 条已知问题，含证据、修复建议与验收条件。
- 新增 `docs/reviews/2026-11-06-NORMALIZATION-AUDIT.md`：本次规范化审计与落地方案。
- 新增 `docs/changes/2026-11-06-docs-normalization.md`：本次变更记录。
- 新增 `AGENTS.md`、`CHANGELOG.md`、`docs/INDEX.md`。
- 新增 `.github/`：CI、Issue 模板、PR 模板。
- 新增 `scripts/check_docs.py`：断链与绝对路径检查。
- 新增 `compile_flags.txt`：跨平台 clangd 编译参数。

### Changed

- 文档目录重组：`docs/ITERATION_*.md` → `docs/iterations/`；`项目日志.md` →
  `docs/history/PROJECT-LOG.md`。
- 重写 `README.md`：修正过期的 Conan 说明、迭代数量、项目结构，补充安全边界与文档导航。
- `.clangd` 移除硬编码的 `/project/CPPReactor` 路径，改由 `compile_flags.txt` 提供相对路径。
- `CMakeLists.txt`：补充 `project(... VERSION 1.2.0)`；默认 `Release`；修复 `main_echo`
  未链接 MySQL 库导致的链接失败；RPATH 改为相对 `$ORIGIN`。
- `.gitignore` 清理失效的 Conan 条目与重复项；`.dockerignore` 排除文档与测试目录。
- `docker-compose.yml` 移除无效的 `seed_data.py` 挂载（MySQL 不会执行 `.py`）。
- 新增 `--enable-sql` 启动开关；未开启时 SQL 配置不会被读取（#1）。
- 新增 `tests/test_sql_security.sh`，CI 覆盖默认关闭、只读护栏与错误参数（#1）。
- `SQLConfig` 字段收敛：移除从未生效的 `driver`、`connMode`、`reconnect*`、`initSqls`、
  `slowQueryMs`、`enableSqlLog`；模板与 `docker-entrypoint.sh` 同步（#4）。
- `--enable-sql` 启动时执行 `SqlConfig::validate()`，空 `host`/`database` 等直接退出（#4）。
- `pingBeforeUse`、`maxConnLifetimeMs`、`maxConnIdleMs` 接入 `ThreadLocalConnOptions`（#4）。

### Fixed

- `tests/run_http_cases.py`：超大体请求在收到 RST 时保留已读字节（413 用例恢复通过）；
  分片 Body 用例改为对 `GET /` 验证，期望值与实际路由语义一致。此前 4 个用例有 2 个失败。
- SQL 异步失败路径闭环：数据库连接/ping 异常回写 500，执行器拒绝任务回 503，不再永久挂起（#3）。
- 移除 `SQLConfig` 中“解析但未生效”的字段；配置字段与行为一一对应（#4）。
- README 中“支持大文件分片传输”“多 Sink”“慢查询日志”等与实现不符的描述已删除或标注限制。

### Docs

- 历史文档显式标注“仅供追溯”，并说明其中的失效路径与结论不再作为操作依据。
- 版本记录从流水日志迁移到本文件；历史版本补打本地 tag（未推送）。
- 新增 [p0/p1 修复记录](./docs/changes/2026-11-06-p0-p1-fixes.md)；BACKLOG 关闭 B-01–B-04。

## [1.2.0] - 2026-06-19

### Added

- `main_echo` Echo 基准服务器（复用 EventLoop/Channel/IOThreadPool，无 HTTP 解析）。
- 压测体系统一为 `tests/bench.sh`，新增 Echo 梯度与线程梯度档位。

### Changed

- 编译优化从 `-O0` 切换为 `-O2 -g`，静态文件 QPS 约 10k → 25k–60k。
- `bench_sql.sh` 线程数从 4 调整为 12。

### Fixed

- `TcpConnection::destory_` → `destroy_`；`HttpRequest::getMethed` → `getMethod`。
- 移除 `IOThreadPool` 未使用成员、`TcpServer` 无效线程成员等 9 处问题。
- `CMakeLists.txt` 改用显式文件列表并新增 `main_echo` 目标。

> 注：本版本迭代日志声称移除了 `response_` 与 `asyncPending_`，但两者仍在
> `include/net/TcpConnection.hpp` 中存在且 `asyncPending_` 被使用；以代码为准。

## [1.1.0] - 2026-06-17

### Changed

- MySQL Connector/C++、nlohmann_json、spdlog 全部内嵌到 `third_party/`。
- 删除 `conanfile.py`，Makefile/CMake/clangd/Dockerfile 改为直接引用 `third_party/`。
- Docker 构建阶段不再安装开发包，利用 `third_party/` 缓存层。

## [1.0.0] - 2026-06-11

### Added

- `HttpRouter`：方法路由、`:param`、前缀匹配、404。
- `IHttpHandler` 统一处理器接口；`StaticFileHandler`/`SqlHandler` 继承。
- Dockerfile、docker-compose、entrypoint，一键部署。
- 全模块 API 契约注释。

### Changed

- 清理约 650 行旧 C 风格代码与 `#if 0` 块。
- `SqlRepository::findUserByIdAsync` 回调接线修复。

## [0.15.0] - 2026-06-02

### Changed

- `MySqlConnection` 从 X DevAPI 切换到 JDBC Connector/C++，兼容 MySQL 5.7。
- 测试脚本从 `scripts/` 迁移到 `tests/`，新增 `bench_sql.sh`、`setup.sh`、`seed_data.py`。
- Makefile 链接库改为 `-lmysqlcppconn`。

## [0.10.0] - 2026-05-19

### Added

- `main.cpp` 接入 SQL 全链路：加载配置 → `SqlExecutor` → `/query`/`/sql` 与静态文件复合路由。
- `seed_data.py` 数据注入脚本（默认 5 万行）。

## [0.9.0] - 2026-05-19

### Added

- `EventLoop::post()` 跨线程回调与 `socketpair` 唤醒。
- 异步响应：`HandlerResult::async`、`TcpConnection::sendAsyncResponse()`。
- `SqlHandler` 完整实现、`StringUtils` 工具模块。

### Changed

- `TcpConnection` 改为 `shared_ptr` 生命周期；`TcpServer::conns_` 同步调整。
- `.clangd` 改为独立配置。

## 0.x 早期版本

| 版本 | 日期 | 主要变化 |
|------|------|---------|
| 0.8.0 | 2026-05-13 | `adapter/` 拆分为 handler/persistence/repository；命名空间与目录对齐 |
| 0.7.0 | 2026-05-03 | 引入 SQL 配置骨架与 Conan（1.1.0 已移除） |
| 0.6.0 | 2026-04-25 | 目录重构到 `include/`/`src/`；新增 CMake |
| 0.5.0 | 2026-04-24 | Keep-Alive、请求超时、增量解析 |
| 0.4.0 | 2026-04-23 | C++ 启动入口、Metrics、压测脚本、spdlog |
| 0.3.0 | 2026-04-22 | 统一 StatusCode、分发器工厂、三后端 |
| 0.2.0 | 2026-04-22 | HttpRequest/HttpResponse C++ 化骨架 |
| 0.1.0 | 2026-04-21 | 首次记录：Channel、EventLoop、Epoll、IOThreadPool |

> 0.11.0–0.14.0 没有留下记录，无法可靠回溯。原始流水见
> [docs/history/PROJECT-LOG.md](./docs/history/PROJECT-LOG.md)。

[Unreleased]: https://github.com/LYsnowQ/CPP_Reactor/compare/v1.2.0...HEAD
[1.2.0]: https://github.com/LYsnowQ/CPP_Reactor/releases/tag/v1.2.0
[1.1.0]: https://github.com/LYsnowQ/CPP_Reactor/releases/tag/v1.1.0
[1.0.0]: https://github.com/LYsnowQ/CPP_Reactor/releases/tag/v1.0.0
