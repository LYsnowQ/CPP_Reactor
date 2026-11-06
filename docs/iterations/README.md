# iterations/ — 迭代历史

> **历史材料，不代表当前实现。** 这些文档记录的是每次迭代“当时”的目标、改动与结论：
>
> - 其中的 `scripts/`、`adapter/`、Conan、X DevAPI 等路径/方案可能早已不存在；
> - 部分结论后来被推翻（例如 ITERATION 29 声称移除了 `asyncPending_`，但
>   `include/net/TcpConnection.hpp` 中仍存在且被使用）；
> - 文档内使用 `file:///project/CPPReactor/...` 这类当时的绝对链接，**保持原样不修改**，
>   以免破坏历史记录的原始性。
>
> 现行事实请看 [docs/architecture/](../architecture/ARCHITECTURE.md)，版本记录看
> [CHANGELOG.md](../../CHANGELOG.md)。

## 索引

版本与日期的精确对应以 [PROJECT-LOG.md](../history/PROJECT-LOG.md) 为准；下表只做阶段归类。

| # | 文档 | 主题 | 阶段 |
|---|------|------|------|
| 01 | [ITERATION_01](ITERATION_01_LIFECYCLE.md) | 连接生命周期闭环 | 0.1–0.9 基础框架 |
| 02 | [ITERATION_02](ITERATION_02_PROTOCOL_STATUS.md) | HTTP 增量解析状态收敛 | 0.1–0.9 |
| 03 | [ITERATION_03](ITERATION_03_HTTP_BODY_CONTENT_LENGTH.md) | Body 最小可用（Content-Length） | 0.1–0.9 |
| 04 | [ITERATION_04](ITERATION_04_BODY_GUARDRAIL.md) | Body 安全收口（长度上限） | 0.1–0.9 |
| 05 | [ITERATION_05](ITERATION_05_BUILD_SYSTEM_FIX.md) | 全量构建链路修复 | 0.1–0.9 |
| 06 | [ITERATION_06](ITERATION_06_TIMEOUT_AND_413.md) | 解析等待超时与 413 语义 | 0.1–0.9 |
| 07 | [ITERATION_07](ITERATION_07_RESPONSE_UNIFICATION.md) | 连接层响应构建统一化 | 0.1–0.9 |
| 08 | [ITERATION_08](ITERATION_08_CLANGD_CONFIG.md) | `.clangd` 配置收敛 | 0.1–0.9 |
| 09 | [ITERATION_09](ITERATION_09_INFLIGHT_PARSER_CONTEXT.md) | 连接级解析上下文（In-Flight Parser） | 0.1–0.9 |
| 10 | [ITERATION_10](ITERATION_10_METRICS_BASELINE.md) | 可观测性与基线压测 | 0.8–0.15 工程化 |
| 11 | [ITERATION_11](ITERATION_11_TEST_FIRST_STEP.md) | 测试体系第一步 | 0.8–0.15 |
| 12 | [ITERATION_12](ITERATION_12_USABILITY_CLOSURE.md) | HTTP 请求/响应闭环修复 | 0.8–0.15 |
| 13 | [ITERATION_13](ITERATION_13_CRASH_CHAIN_ANALYSIS.md) | 压测崩溃链路排查 | 0.8–0.15 |
| 14 | [ITERATION_14](ITERATION_14_DEFERRED_RECYCLE.md) | 延迟回收与生命周期收敛 | 0.8–0.15 |
| 15 | [ITERATION_15](ITERATION_15_HTTP_RESPONSE_BOUNDARY.md) | 响应边界语义收敛 | 0.8–0.15 |
| 16 | [ITERATION_16](ITERATION_16_CLOSE_TIMING_EXPERIMENT.md) | 关闭时序实验与回退 | 0.8–0.15 |
| 17 | [ITERATION_17](ITERATION_17_CONNECTION_MODE_SWITCH.md) | close/keepalive 开关 | 0.8–0.15 |
| 18 | [ITERATION_18](ITERATION_18_KEEPALIVE_GUARDRAILS.md) | keepalive 护栏 | 0.8–0.15 |
| 19 | [ITERATION_19](ITERATION_19_REACTOR_HANDLER_EXTENSION.md) | 业务处理器扩展点 | 0.8–0.15 |
| 20 | [ITERATION_20](ITERATION_20_STATIC_BROWSER_DEMO.md) | 静态目录浏览示例 | 0.8–0.15 |
| 21 | [ITERATION_21](ITERATION_21_ADAPTER_SPLIT_AND_ECOSYSTEM.md) | 适配器拆分与生态选型 | 0.8–0.15 |
| 22 | [ITERATION_22](ITERATION_22_ASYNC_SQL_HANDLER_AND_CROSS_THREAD_CALLBACK.md) | 异步 SQL 与跨线程回调 | SQL 集成 |
| 23 | [ITERATION_23](ITERATION_23_MAIN_SQL_WIRING_AND_SEED_SCRIPT.md) | 主入口 SQL 接线与数据注入 | SQL 集成 |
| 24 | [ITERATION_24](ITERATION_24_JDBC_REWRITE_AND_BENCHMARK_SUITE.md) | JDBC 重构与压测体系 | SQL 集成 |
| 25 | [ITERATION_25](ITERATION_25_DEAD_CODE_CLEANUP_AND_STUB_DESIGN.md) | 死代码清理与空桩设计 | 1.0.0 |
| 26 | [ITERATION_26](ITERATION_26_STUB_COMPLETION_AND_DEMO_FINALIZE.md) | 空桩完成与 Demo 收口 | 1.0.0 |
| 27 | [ITERATION_27](ITERATION_27_CONTRACT_WALL_IMPL_AND_DOCKER.md) | 注释契约墙与 Docker | 1.0.0 |
| 28 | [ITERATION_28](ITERATION_28_THIRD_PARTY_SELF_CONTAINED.md) | 第三方库自包含 | 1.1.0 |
| 29 | [ITERATION_29](ITERATION_29_PERFORMANCE_OPTIMIZATION_AND_BENCHMARK_ANALYSIS.md) | 性能优化与压测分析 | 1.2.0 |

## 使用方式

- 想了解“为什么某处代码长这样”：先看相邻编号的迭代记录，再看
  [decisions/](../decisions/README.md)；
- 想确认“现在是什么行为”：不要引用这里，改看 `docs/architecture/` 与代码；
- 发现历史结论与代码不符：在 `docs/reviews/` 记录，不需要回头改写历史文档。
