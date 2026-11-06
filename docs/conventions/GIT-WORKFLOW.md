# GIT-WORKFLOW.md — 分支、提交、Issue 与 PR

本规范由 admin 项目的多人流程裁剪而来：**保留可追溯性，去掉审批层级**。

## 1. 分支

| 分支 | 用途 | 生命周期 |
|------|------|---------|
| `main` | 唯一长期分支，始终可编译、测试通过 | 永久 |
| `feat/<slug>` | 新功能 | 合并后删除 |
| `fix/<slug>` | 缺陷修复 | 合并后删除 |
| `docs/<slug>` | 文档/规范重构 | 合并后删除 |
| `chore/<slug>` | 构建、CI、依赖整理 | 合并后删除 |

- 不设 `stage`/`release` 分支：单人项目没有预发布环境。
- **可以直接提交到 `main` 的情况**：纯文档、typo、单文件注释、CHANGELOG。
- **必须开分支的情况**：改行为、改构建/配置、跨 3 个以上文件、BACKLOG 中登记的事项。
- 任何操作前先 `git status` 与 `git branch --show-current`，确认自己在哪。
- 禁止对 `main` 强推、禁止改写已推送历史、禁止删除已发布 tag。

## 2. 提交信息

格式（Conventional Commits，说明用中文）：

```
<type>(<scope>): <一句话目的>

- 为什么改、改了什么、影响范围
- 验证方式（命令/环境）

Refs: #12, BACKLOG B-02
```

| type | 用途 |
|------|------|
| `feat` | 新能力 |
| `fix` | 修 bug |
| `docs` | 文档 |
| `build` | 构建、依赖、CMake/Make |
| `test` | 测试与测试脚本 |
| `refactor` | 不改变行为的重构 |
| `perf` | 性能 |
| `chore` | CI、模板、杂项 |

`scope` 取值：`core`、`net`、`http`、`sql`、`handler`、`build`、`docker`、`docs`、`ci`。

要求：

- 一次提交只做一件事，能单独回滚；
- 标题不超过 72 字符，不以句号结尾；
- 不提交生成物（`build/`、`benchmarks/*.md`、真实配置）；
- 不在提交信息里粘贴密钥。

## 3. Issue

### 何时必须有 issue

- 行为变更或 bug 修复（含 BACKLOG 中的既有条目）；
- 构建/配置/依赖调整；
- 需要跨多次提交完成的事项。

纯 typo、单篇文档修正、发版提交可以没有 issue。

### 流程

1. 先在 [BACKLOG](../plan/BACKLOG.md) 检索是否已有条目；
2. 新建 issue，使用 `.github/ISSUE_TEMPLATE/` 模板，必须写清：现象/目标、复现或验收条件、
   影响范围、验证方式；
3. 分支名带 issue 号：`fix/12-sql-hang`；
4. 提交与 PR 用 `Refs: #12`；
5. 合并后关闭 issue，并在评论里留一行验证结论（命令 + 结果）。

### 标签

| 维度 | 取值 |
|------|------|
| 类型 | `bug` / `enhancement` / `docs` / `build` |
| 优先级 | `p0`（安全/不可用）、`p1`（主要功能受损）、`p2`（一般）、`p3`（优化） |
| 区域 | `area:core` `area:net` `area:http` `area:sql` `area:build` `area:docs` |

## 4. Pull Request

- 单人项目不强制 PR；只要开了分支，就建议开 PR 留下验证证据。
- PR 描述使用 [模板](../../.github/pull_request_template.md)，至少包含：
  目的、关联 issue、改动点、验证命令与结果、文档同步情况、风险与回滚方式。
- 合并方式：squash merge，保持 `main` 线性、一条提交一个目的。
- 合并前：CI 绿、docs 检查通过、CHANGELOG 已补。
- 不在 PR 里混入无关格式化或重构。

## 5. 常用命令

```bash
git switch -c fix/12-sql-hang
# ... 修改 ...
make clean && make -j"$(nproc)" && python3 scripts/check_docs.py
git add -A && git commit -m "fix(sql): 数据库连接失败时返回 500 而不是挂起"
git switch main && git merge --squash fix/12-sql-hang
git commit
git branch -d fix/12-sql-hang
```
