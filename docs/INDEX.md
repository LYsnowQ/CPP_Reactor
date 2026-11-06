# 文档索引

> 本索引**手工维护**：新增、移动或删除文档时必须同步更新，并用
> `python3 scripts/check_docs.py` 校验链接。
>
> 入口：[README](../README.md) · [AGENTS](../AGENTS.md) · [CHANGELOG](../CHANGELOG.md)

## 根目录

| 文档 | 说明 |
|------|------|
| [README.md](../README.md) | 项目介绍、快速开始、配置/接口入口、文档导航 |
| [README.en.md](../README.en.md) | English README: overview, quick start, configuration, API, license |
| [AGENTS.md](../AGENTS.md) | AI / 协作者进入仓库的必读入口与硬性约束 |
| [CHANGELOG.md](../CHANGELOG.md) | 版本变更记录（SemVer） |
| [LICENSING.md](../LICENSING.md) | 许可边界、附加免费再分发许可、商业授权说明 |
| [LICENSING.en.md](../LICENSING.en.md) | English licensing summary |
| [LICENSE](../LICENSE) | 现行许可：PolyForm Noncommercial 1.0.0 |
| [LICENSE-MIT](../LICENSE-MIT) | 历史 MIT 原文（截至 `be3bc0b`） |

## conventions/ — 强制规范

| 文档 | 说明 |
|------|------|
| [README](conventions/README.md) | 规范索引与效力说明 |
| [PROJECT](conventions/PROJECT.md) | 项目定位、非目标、安全边界、完成定义 |
| [DOCUMENTATION](conventions/DOCUMENTATION.md) | 文档分类、存放、链接规则、新鲜度与审查 |
| [VERSIONING](conventions/VERSIONING.md) | SemVer、tag、CHANGELOG、发版流程 |
| [GIT-WORKFLOW](conventions/GIT-WORKFLOW.md) | 分支、提交、issue、PR |
| [TESTING](conventions/TESTING.md) | 测试分层、命令、压测规范、验证结论格式 |
| [CODE-REVIEW](conventions/CODE-REVIEW.md) | 按风险域组织的自审清单 |

## architecture/ — 当前系统事实

| 文档 | 说明 |
|------|------|
| [ARCHITECTURE](architecture/ARCHITECTURE.md) | 运行单元、模块职责、线程模型、请求/连接生命周期、扩展点、限制 |
| [CONFIGURATION](architecture/CONFIGURATION.md) | 路径语义、命令行参数、SQL 配置字段是否生效、Docker 环境变量 |
| [HTTP-API](architecture/HTTP-API.md) | 路由、状态码、SQL 接口格式、安全边界与已知缺陷 |

## decisions/ — 架构决策

| 文档 | 说明 |
|------|------|
| [README](decisions/README.md) | 决策索引与状态约定 |
| [0001](decisions/0001-reactor-multi-dispatcher.md) | Reactor 多线程 + epoll 默认 + 多后端 |
| [0002](decisions/0002-vendored-third-party.md) | 第三方库自包含，移除 Conan |
| [0003](decisions/0003-sql-endpoint-demo-only.md) | `/sql` 仅作演示端点，带安全技术债 |
| [0004](decisions/0004-license-noncommercial.md) | 改为 PolyForm Noncommercial，历史 MIT 保留，免费再分发附加许可 |

## plan/ — 计划与待办

| 文档 | 说明 |
|------|------|
| [BACKLOG](plan/BACKLOG.md) | 已迁移到 GitHub Issues 的说明与迁移记录（不再维护逐条详情） |

## reviews/ — 审计与验收

| 文档 | 说明 |
|------|------|
| [2026-11-06 规范化审计](reviews/2026-11-06-NORMALIZATION-AUDIT.md) | 现状评估、admin 规则取舍、问题清单、落地方案、分期计划 |

## changes/ — 变更记录

| 文档 | 说明 |
|------|------|
| [2026-11-06 文档规范化](changes/2026-11-06-docs-normalization.md) | 本次结构迁移、修复项、验证证据、未完成事项 |
| [2026-11-06 p0/p1 修复](changes/2026-11-06-p0-p1-fixes.md) | issue #1–#4 的实现、验证与遗留 |
| [2026-11-06 许可变更](changes/2026-11-06-license-change.md) | 改为非商业许可、历史 MIT 边界、XERJ 收录请求的处置 |
| [2026-11-06 管理转型](changes/2026-11-06-management-transition.md) | issue-first、PR 流程、GitHub Issues 成为唯一待办主档 |
| [2026-11-06 发布 2.0.0](changes/2026-11-06-release-2.0.0.md) | 版本号、Breaking Changes、tag 与 Release |
| [2026-11-06 英文文档](changes/2026-11-06-english-docs.md) | 英文 README / 许可摘要与中英文互链 |

## iterations/ — 迭代历史（只读）

| 文档 | 说明 |
|------|------|
| [README](iterations/README.md) | 29 次迭代的索引与阅读须知 |

单篇文件为 `ITERATION_01_*.md` … `ITERATION_29_*.md`，从
[iterations/README](iterations/README.md) 进入。**历史材料，不代表当前实现。**

## history/ — 原始记录（只读）

| 文档 | 说明 |
|------|------|
| [PROJECT-LOG](history/PROJECT-LOG.md) | 2026-04-21 至 2026-06-19 的原始开发流水 |

## tests/ — 测试说明

| 文档 | 说明 |
|------|------|
| [tests/README](../tests/README.md) | 测试目录、执行顺序、脚本与档位 |
| [BENCHMARK_REPORT](../tests/BENCHMARK_REPORT.md) | 2026-06-19 的基准压测报告（环境绑定） |
