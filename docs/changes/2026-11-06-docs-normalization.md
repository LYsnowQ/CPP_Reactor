# 2026-11-06 文档与配置规范化

- 类型：docs / build / test
- 关联：[审计报告](../reviews/2026-11-06-NORMALIZATION-AUDIT.md)、
  [BACKLOG](../plan/BACKLOG.md)
- 基线：`main` @ `483b7b1`（v1.2.0）
- 版本 tag 与远端 issue 均未创建；分支推送状态以远端为准

## 目的

把长期以“学习笔记”方式维护的仓库，整理为**文档有分类、配置有事实、版本有记录、问题有追踪**的
规范项目；同时保持单人维护的成本可控。

## 结构迁移

| 旧路径 | 新路径 | 说明 |
|--------|--------|------|
| `docs/ITERATION_01_*.md` … `_29_*.md` | `docs/iterations/` | 内容不变，保留历史绝对链接 |
| `项目日志.md` | `docs/history/PROJECT-LOG.md` | 加历史声明，原文保留 |
| — | `docs/conventions/` | 6 篇规范 |
| — | `docs/architecture/` | 架构、配置、HTTP 接口 |
| — | `docs/decisions/` | 3 条 ADR |
| — | `docs/plan/BACKLOG.md` | 12 条待办 |
| — | `docs/reviews/` | 本审计 |
| — | `docs/changes/` | 本文件 |

## 新增文件

- 根：`AGENTS.md`、`CHANGELOG.md`、`compile_flags.txt`
- 文档：`docs/INDEX.md`、规范 6 篇、架构 3 篇、决策 3 篇 + 索引、BACKLOG、审计、本记录
- 工程：`.github/workflows/ci.yml`、`.github/ISSUE_TEMPLATE/{bug,feature,config}.yml`、
  `.github/pull_request_template.md`、`scripts/check_docs.py`

## 修复项

| 项 | 修改 |
|----|------|
| README 失真 | 重写；删除 Conan、27+、分片传输、多 Sink、慢查询等错误声明；补安全边界 |
| `.clangd` | 删除 `/project/CPPReactor` 绝对路径；改用 `compile_flags.txt` |
| CMake | `VERSION 1.2.0`、默认 Release、`main_echo` 链接 MySQL、相对 RPATH |
| compose | 删除无效的 `.py` initdb 挂载；统一 root 账号（与 README 一致） |
| `.gitignore` | 清理 Conan/重复项/不存在的 docs 忽略 |
| `.dockerignore` | 排除 `docs/`、`tests/`、`.github/` |
| HTTP 用例 | 413 保留 RST 前字节；分片用例改为 `GET /`；4/4 通过 |

## 有意不做

- 不实现 `/sql` 鉴权、不把配置移出静态根（p0，需要单独设计与测试，见 B-01/B-02）；
- 不修数据库失败挂起（见 B-03）；
- 不补单元测试（见 B-09）；
- 不改写历史迭代文档中的 `file:///` 链接与失效路径；
- 不创建远端 issue、不推送 tag。

## 验证

| 项 | 命令 | 结果 |
|----|------|------|
| Make 构建 | `make clean && make -j8` | 成功，0 warning |
| CMake 构建 | `cmake -B build-cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build-cmake` | 两目标成功（修复后） |
| 冒烟 | `bash tests/test_smoke.sh http://127.0.0.1:8080/` | PASS |
| HTTP 用例 | `python3 tests/run_http_cases.py 127.0.0.1 8080` | 4/4 |
| 文档 | `python3 scripts/check_docs.py` | 通过 |
| 数据库 | 未执行 | 本机无 MySQL，未验证 |

环境：WSL2 Ubuntu 22.04.5 / g++ 11.4.0 / 20 线程。

## 回滚

- 文档迁移可用 `git revert` 单个提交恢复；
- 本地 tag 不影响代码，可 `git tag -d v1.0.0 v1.1.0 v1.2.0` 删除；
- 构建/配置修改均为独立小 diff，可逐文件回退。

## 后续

按 [BACKLOG](../plan/BACKLOG.md) 优先级推进；下一轮先处理 B-01/B-02/B-03/B-04。
