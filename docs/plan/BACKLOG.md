# BACKLOG — 已知问题与后续计划

> 这里只放**尚未完成**的事项。每条都可以直接转成 GitHub issue：标题即 issue 标题，
> 正文包含证据、修复建议与验收条件。优先级定义见
> [GIT-WORKFLOW.md](../conventions/GIT-WORKFLOW.md#标签)。
>
> 最后更新：2026-11-06。已修复项见
> [本次变更记录](../changes/2026-11-06-docs-normalization.md)。

## 总览

| ID | 优先级 | 主题 | 区域 |
|----|--------|------|------|
| [B-01](#b-01) | p0 | `/sql` 执行任意 SQL，`readOnly` 未生效 | security/sql |
| [B-02](#b-02) | p0 | 静态根目录可下载 `config/SQLConfig.json` | security/http |
| [B-03](#b-03) | p1 | 数据库连接失败时 `/sql` 永久挂起 | sql/net |
| [B-04](#b-04) | p1 | `SQLConfig` 多数字段未生效，启动校验未接线 | sql/config |
| [B-05](#b-05) | p2 | Docker 初始化数据挂载无效、容器以 root 运行 | docker |
| [B-06](#b-06) | p2 | 无优雅停机与信号处理 | core/net |
| [B-07](#b-07) | p2 | 日志级别固定、无文件 sink、无指标导出 | observability |
| [B-08](#b-08) | p2 | 静态文件整文件读入，无 Range/分片 | http |
| [B-09](#b-09) | p2 | 无单元测试，HTTP 用例覆盖不足 | test |
| [B-10](#b-10) | p2 | 性能数据与单机环境绑定，无自动基准 | perf |
| [B-11](#b-11) | p3 | 命令行参数无范围校验 | cli |
| [B-12](#b-12) | p3 | 无 `--help` / `--version` | cli |

---

## B-01

**`/sql` 执行任意 SQL，`readOnly` 未生效** · p0 · security/sql

- 证据：`src/handler/SqlHandler.cpp:143-157` 只校验 GET；`src/persistence/SqlConfig.cpp:182`
  解析 `readOnly`，全仓库无任何强制执行点。
- 影响：任何能访问端口的人可执行 `DROP TABLE` 等语句；配置给人“只读”的错觉。
- 修复建议（任选其一，需先写 ADR 更新）：
  1. 默认拒绝非 `SELECT`/`SHOW`/`DESCRIBE` 语句；
  2. 为 SQL 端点增加显式启动开关，默认关闭；
  3. 使用只读数据库账号，并在文档中说明这是唯一可靠手段。
- 验收：写语句返回 4xx；`readOnly` 要么真正生效、要么从配置与文档中删除；补充端到端用例。

## B-02

**静态根目录可下载 `config/SQLConfig.json`** · p0 · security/http

- 证据：以仓库根启动后 `curl /config/SQLConfig.json` 返回 200（2026-11-06 实测）。
- 影响：一旦填入真实数据库密码，密码可被任意下载；源码、`third_party/` 同样可下载。
- 修复建议：静态处理器增加拒绝名单（`config/`、`.git/`、隐藏文件）；或把配置移出静态根；
  或在文档与启动日志中强制警告。
- 验收：`/config/SQLConfig.json` 返回 403/404；合法静态文件不受影响；新增用例。

## B-03

**数据库连接失败时 `/sql` 永久挂起** · p1 · sql/net

- 证据：`SqlExecutor::run_()` 捕获异常仅 `spdlog::warn`，不回调连接；模板配置下实测
  `curl --max-time 8` 超时，日志出现 `SqlExecutor job failed: sql connect failed`。
- 影响：客户端连接与线程资源被长期占用；错误不可见。
- 修复建议：在 `SqlHandler` 提交的 job 内 `try/catch`，失败时 `loop->post` 回写 500；
  或在 `SqlExecutor` 提供错误回调。
- 验收：DB 不可达时 5 秒内返回 500 JSON；连接可正常关闭；新增用例。

## B-04

**`SQLConfig` 多数字段未生效，启动校验未接线** · p1 · sql/config

- 证据：`src/app/main.cpp:72-85` 使用通用 loader 并传入 `ThreadLocalConnOptions{}`；
  `loadSqlConfigFromFileOrThrow()` 与 `validate()` 从未被调用。未生效字段：
  `driver`、`connMode`、`pingBeforeUse`、`reconnectMaxAttempts`、`reconnectBackoffMs`、
  `maxConnLifetimeMs`、`maxConnIdleMs`、`initSqls`、`slowQueryMs`、`enableSqlLog`、`readOnly`。
- 影响：改配置没有任何效果，排查成本高；空 host 也能启动。
- 修复建议：把 `ThreadLocalConnOptions` 由 `SqlConfig` 派生；启动调用
  `loadSqlConfigFromFileOrThrow`；对暂不实现的字段要么删掉、要么在文档标注。
- 验收：配置项与行为一一对应；空 host 启动即失败并给出明确错误；文档字段表无 ❌。

## B-05

**Docker 初始化数据挂载无效、容器以 root 运行** · p2 · docker

- 证据：`docker-compose.yml:15` 把 `seed_data.py` 挂到 `/docker-entrypoint-initdb.d/`，
  MySQL 官方镜像只执行 `.sh`/`.sql`；容器未声明 `USER`；compose 创建了未被使用的 `user` 账号。
- 影响：以为自动注入数据，实际没有；容器权限过大。
- 修复建议：删除无效挂载并在文档说明手动注入；统一账号；增加非 root 用户与只读文件系统评估。
- 验收：`docker compose up` 后按文档能注入数据；`docker compose config` 无无用账号；
  `docker run` 默认非 root。

## B-06

**无优雅停机与信号处理** · p2 · core/net

- 证据：`src/app/main.cpp` 未注册信号处理；`TcpServer::run()` 阻塞 accept 循环。
- 影响：Ctrl-C 直接终止，连接与 SQL 任务无收尾；容器 stop 依赖 SIGKILL 兜底。
- 修复建议：捕获 SIGINT/SIGTERM，停止 accept、关闭监听 fd、停止线程池与 SqlExecutor。
- 验收：发送 SIGTERM 后进程在 3 秒内退出，日志显示关闭流程。

## B-07

**日志级别固定、无文件 sink、无指标导出** · p2 · observability

- 证据：`src/app/main.cpp:60` 固定 `debug`；全仓库无 sink 配置；`Metrics` 仅每 5 秒打印。
- 影响：无法按环境调整噪声；无法接入监控。
- 修复建议：命令行/环境变量设置级别；可选文件 sink；增加 `/metrics`（需先评估暴露面）。
- 验收：启动参数可控制级别；文档更新；默认行为不变。

## B-08

**静态文件整文件读入，无 Range/分片** · p2 · http

- 证据：`src/handler/StaticFileHandler.cpp:317-326` 使用 `istreambuf_iterator` 读入 `std::string`。
- 影响：大文件占用内存，无法断点续传。
- 修复建议：改为流式写 + `sendfile`/分块，增加 `Range` 与 `Content-Length`。
- 验收：1 GB 文件可下载且内存稳定；Range 请求返回 206。

## B-09

**无单元测试，HTTP 用例覆盖不足** · p2 · test

- 证据：仅 `tests/run_http_cases.py` 4 个用例；无 C++ 测试目标。
- 影响：重构回归依赖手工；Keep-Alive、408、403、405、异步错误路径无覆盖。
- 修复建议：先补端到端用例（成本低），再为核心纯函数（`HttpRequest` 解析、`HttpRouter`、
  `Buffer`）引入轻量单元测试框架。
- 验收：CI 覆盖上述路径；失败能定位到具体用例。

## B-10

**性能数据与单机环境绑定，无自动基准** · p2 · perf

- 证据：`tests/BENCHMARK_REPORT.md` 为 2026-06-19 单次环境数据。
- 影响：后续改动无法判断是否退化。
- 修复建议：CI 中增加非阻断的 smoke benchmark（固定短时长），或在本地用脚本对比基线；
  报告必须带 commit 与环境。
- 验收：每次性能相关 PR 附对比表；基线口径固定。

## B-11

**命令行参数无范围校验** · p3 · cli

- 证据：`src/app/main.cpp:42-57` 直接 `atoi` 后强转；`threads=0`、`port=0` 未拦截。
- 修复建议：解析后校验范围并打印明确错误。
- 验收：非法参数退出码非 0，错误信息可读。

## B-12

**无 `--help` / `--version`** · p3 · cli

- 证据：参数不足时只打印一行 Usage；版本号未编译进二进制。
- 修复建议：支持 `--help`/`--version`，版本取自 CMake `project(... VERSION)`。
- 验收：两个参数可用，版本与 CHANGELOG 一致。
