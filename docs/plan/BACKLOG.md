# BACKLOG — 已知问题与后续计划

> 这里只放**尚未完成**的事项。每条都可以直接转成 GitHub issue：标题即 issue 标题，
> 正文包含证据、修复建议与验收条件。优先级定义见
> [GIT-WORKFLOW.md](../conventions/GIT-WORKFLOW.md#标签)。
>
> 最后更新：2026-11-06。p0/p1 修复过程见
> [变更记录](../changes/2026-11-06-p0-p1-fixes.md)。

## 总览（未关闭）

| ID | 优先级 | 主题 | 区域 |
|----|--------|------|------|
| [B-05](#b-05) | p2 | 容器以 root 运行、无健康检查 | docker |
| [B-06](#b-06) | p2 | 无优雅停机与信号处理 | core/net |
| [B-07](#b-07) | p2 | 日志级别固定、无文件 sink、无指标导出 | observability |
| [B-08](#b-08) | p2 | 静态文件整文件读入，无 Range/分片 | http |
| [B-09](#b-09) | p2 | 无单元测试，端到端覆盖仍不足 | test |
| [B-10](#b-10) | p2 | 性能数据与单机环境绑定，无自动基准 | perf |
| [B-11](#b-11) | p3 | 命令行参数无范围校验 | cli |
| [B-12](#b-12) | p3 | 无 `--version` | cli |

## 已关闭

| ID | 主题 | Issue | 合并提交 |
|----|------|-------|---------|
| B-01 | `/sql` 任意 SQL；`readOnly` 未生效 | [#1](https://github.com/LYsnowQ/CPP_Reactor/issues/1) | `429dcaf` |
| B-02 | 静态根可下载 `config/SQLConfig.json` | [#2](https://github.com/LYsnowQ/CPP_Reactor/issues/2) | `305dea2` |
| B-03 | 数据库不可达时 `/sql` 永久挂起 | [#3](https://github.com/LYsnowQ/CPP_Reactor/issues/3) | `3fefb06` |
| B-04 | `SQLConfig` 多数字段未生效、校验未接线 | [#4](https://github.com/LYsnowQ/CPP_Reactor/issues/4) | `221df66` |

---

## B-05

**容器以 root 运行、无健康检查** · p2 · docker

- 现状：`docker-compose.yml` 中无效的 `seed_data.py` initdb 挂载与无用账号已在 2026-11-06 清理；
  容器仍未声明 `USER`，也没有应用级健康检查。
- 影响：容器权限过大；编排无法判断应用是否真正可用。
- 修复建议：增加非 root 用户与文件权限；评估只读根文件系统；增加 `HEALTHCHECK`。
- 验收：`docker inspect` 显示非 root；`docker compose ps` 健康状态可用；静态服务仍正常。

## B-06

**无优雅停机与信号处理** · p2 · core/net

- 证据：`src/app/main.cpp` 未注册信号处理；`TcpServer::run()` 阻塞 accept 循环。
- 影响：Ctrl-C 直接终止，连接与 SQL 任务无收尾；容器 stop 依赖 SIGKILL 兜底。
- 修复建议：捕获 SIGINT/SIGTERM，停止 accept、关闭监听 fd、停止线程池与 SqlExecutor。
- 验收：发送 SIGTERM 后进程在 3 秒内退出，日志显示关闭流程。

## B-07

**日志级别固定、无文件 sink、无指标导出** · p2 · observability

- 证据：`src/app/main.cpp` 固定 `spdlog::set_level(debug)`；全仓库无 sink 配置；
  `Metrics` 仅每 5 秒打印窗口日志。
- 影响：无法按环境调整噪声；无法接入监控。
- 修复建议：命令行/环境变量设置级别；可选文件 sink；增加 `/metrics`（需先评估暴露面）。
- 验收：启动参数可控制级别；文档更新；默认行为不变。

## B-08

**静态文件整文件读入，无 Range/分片** · p2 · http

- 证据：`StaticFileHandler::handle()` 使用 `istreambuf_iterator` 把整个文件读入 `std::string`。
- 影响：大文件占用内存，无法断点续传。
- 修复建议：改为流式写 + `sendfile`/分块，增加 `Range` 与 `Content-Length`。
- 验收：1 GB 文件可下载且内存稳定；Range 请求返回 206。

## B-09

**无单元测试，端到端覆盖仍不足** · p2 · test

- 现状：`run_http_cases.py` 9 条、`test_sql_security.sh` 9 条；仍无 C++ 单元测试目标。
- 未覆盖：Keep-Alive 超时、408、405、符号链接越界组合、真实数据库成功查询。
- 修复建议：先补端到端用例（成本低），再为核心纯函数（`HttpRequest` 解析、`HttpRouter`、
  `Buffer`、`isReadOnlyStatement`）引入轻量单元测试框架。
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

- 证据：`main.cpp` 直接 `atoi` 后强转；`threads=0`、`port=0` 未拦截。
- 修复建议：解析后校验范围并打印明确错误。
- 验收：非法参数退出码非 0，错误信息可读。

## B-12

**无 `--version`** · p3 · cli

- 现状：`--help` / `-h` 已实现（2026-11-06）；版本号仍未编译进二进制。
- 修复建议：支持 `--version`，版本取自 CMake `project(... VERSION)` 或编译宏。
- 验收：版本输出与 CHANGELOG/最新 tag 一致。
