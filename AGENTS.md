# AGENTS.md — AI / 协作者入口

进入本仓库后，**先读完本文件，再按顺序读取适用文档**，不要凭记忆或猜测动手。

## 1. 必读顺序

1. 本文件；
2. [docs/INDEX.md](./docs/INDEX.md)：文档总索引；
3. [docs/conventions/DOCUMENTATION.md](./docs/conventions/DOCUMENTATION.md)：文档与同步规则；
4. [docs/conventions/GIT-WORKFLOW.md](./docs/conventions/GIT-WORKFLOW.md)：分支/提交/issue；
5. 按改动区域读：
   - 启动参数、配置 → [docs/architecture/CONFIGURATION.md](./docs/architecture/CONFIGURATION.md)
   - 路由、状态码 → [docs/architecture/HTTP-API.md](./docs/architecture/HTTP-API.md)
   - 模块、线程、扩展点 → [docs/architecture/ARCHITECTURE.md](./docs/architecture/ARCHITECTURE.md)
   - 已知问题 → [docs/plan/BACKLOG.md](./docs/plan/BACKLOG.md)

## 2. 项目事实速览

- Linux / WSL2 项目；Windows 原生不能编译（依赖 epoll、POSIX API）。
- 构建：`make clean && make -j"$(nproc)"`；产物 `build/main_run`、`build/main_echo`。
- 启动：`./build/main_run 8080 . epoll 4 keepalive 100 10000`（必须在仓库根目录，否则找不到配置）；
  需要 SQL 演示端点时追加 `--enable-sql`。
- 测试：`bash tests/test_smoke.sh http://127.0.0.1:8080/`、
  `python3 tests/run_http_cases.py 127.0.0.1 8080`、
  `bash tests/test_sql_security.sh`、`python3 scripts/check_docs.py`。
- 版本：`1.2.0`，版本记录在 [CHANGELOG.md](./CHANGELOG.md)；tag 未推送。

## 3. 硬性约束（违反即退回）

- **不提交真实凭据**：`config/SQLConfig.json` 只保留空模板。
- **不修改 `third_party/`** 中的头文件与 `.so`；升级依赖是独立任务，需要 ADR 与验证。
- **不推送、不打 tag、不创建 GitHub Release、不在远端建/关 issue**：这些需要所有者明确同意。
- **不改写已推送历史**、不强推 `main`、不删除已发布 tag。
- **不把 `/sql` 暴露到公网**，也不在文档中声称它“安全”或“只读”。
- **不新增绝对路径**（`file:///`、`/project/...`、`C:\...`）；用相对链接与 `路径:行号`。
- **不为了变绿而删断言/跳过测试**；先判断是实现问题还是用例问题。
- **不复制配置表**：默认值与字段说明只维护在 `CONFIGURATION.md`，其他文档链接过去。

## 4. 改动流程

1. `git status` + `git branch --show-current`，确认工作区干净、所在分支正确；
2. 非平凡改动先在 BACKLOG 查重，必要时开 issue；
3. 从 `main` 切分支：`fix/<issue>-<slug>`、`feat/<slug>`、`docs/<slug>`；
4. 改代码 → 同步受影响的文档（见 DOCUMENTATION 的新鲜度表）；
5. 跑编译 + 冒烟 + HTTP 用例 + 文档检查；
6. 更新 `CHANGELOG.md` 的 `[Unreleased]`；
7. 提交：`type(scope): 目的`，正文写“为什么 + 验证方式”；
8. 合并回 `main`（squash），删除分支。

## 5. 已知陷阱

| 陷阱 | 正确做法 |
|------|---------|
| 在 Windows 直接 `make` | 用 WSL2（Ubuntu 22.04）或 Linux |
| 在非仓库根目录启动 | 配置按启动时工作目录解析，会找不到 `config/SQLConfig.json` |
| 以为 `/sql` 默认可用 | 默认不注册，需要追加 `--enable-sql` |
| 以为 `--enable-sql` 就等于安全 | `readOnly` 只提供启发式语句护栏，仍需只读数据库账号与网络隔离 |
| 以为 `/sql` 出错会返回 500 | 数据库连接失败会挂起；见 BACKLOG B-03 |
| 以为 CMake 与 Make 等价 | CMake 的 `main_echo` 曾漏链 MySQL 库（已修）；改构建时两边都验证 |
| 以为静态根只服务静态文件 | `config/`/`.git`/点文件已拒绝，但其他源码与文档仍可下载 |
| 直接改历史迭代文档 | 历史材料不改写；新结论写进 `docs/reviews/` 或 `docs/architecture/` |

## 6. 记录写在哪里

| 内容 | 位置 |
|------|------|
| 面向使用者的变更 | `CHANGELOG.md` |
| 非平凡改动的过程与验证 | `docs/changes/YYYY-MM-DD-<slug>.md` |
| 尚未完成的缺口 | `docs/plan/BACKLOG.md` |
| 架构选型 | `docs/decisions/` |
| 审计/验收证据 | `docs/reviews/` |
| 历次迭代 | `docs/iterations/`（只读） |

## 7. 完成检查

- [ ] `make` 零错误零警告；
- [ ] 冒烟 + 9 个 HTTP 用例 + SQL 安全用例通过；
- [ ] `python3 scripts/check_docs.py` 通过；
- [ ] 受影响的 README / architecture 文档已更新；
- [ ] CHANGELOG 已补；
- [ ] 没有新增绝对路径、凭据、生成物。
