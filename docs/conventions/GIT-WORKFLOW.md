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
- **所有会改变行为、构建、配置或文档结构的改动必须走 PR**（issue → 分支 → PR → squash → 删除分支）。
- **可以直接提交 `main` 的例外**：typo、单篇文档修正、`chore(release)` 发布提交；仍需本地验证。
- 任何操作前先 `git status` 与 `git branch --show-current`，确认自己在哪。
- `main` 应开启分支保护：require PR、require CI、linear history、禁止 force push
  （见 issue #17）；保护生效前靠人工遵守。
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

- 行为变更或 bug 修复；
- 构建/配置/依赖调整；
- 需要跨多次提交完成的事项；
- 安全、许可、发布相关变更。

纯 typo、单篇文档修正、发版提交可以没有 issue。

### 流程

1. 先搜索 GitHub Issues（`gh issue list --search ...`）确认没有重复；**GitHub Issues 是唯一主档**，
   不再在本地 BACKLOG 维护条目；
2. 新建 issue，使用 `.github/ISSUE_TEMPLATE/` 模板，必须写清：现象/目标、复现或验收条件、
   影响范围、验证方式；
3. 分支名带 issue 号：`fix/12-sql-hang`；
4. 提交用 `Refs: #12`，PR 正文用 `Closes #12`；
5. 合并后由 PR 自动关闭 issue，并在 PR/issue 留验证结论（命令 + 结果）。

### 标签

| 维度 | 取值 |
|------|------|
| 类型 | `bug` / `enhancement` / `documentation` / `legal` / `security` |
| 优先级 | `p0`（安全/不可用）、`p1`（主要功能受损）、`p2`（一般）、`p3`（优化） |
| 区域 | `area:core` `area:net` `area:http` `area:sql` `area:docker` `area:observability` `area:test` `area:perf` `area:cli` `area:ci` |

## 4. Pull Request

- **非平凡改动必须走 PR**；直接 push `main` 只允许 §1 的例外。
- PR 正文使用 [模板](../../.github/pull_request_template.md)，必须包含：
  目的、`Closes #N`、改动点、验证命令与结果、文档同步、风险与回滚。
- 合并前：CI 两个 job 绿、文档检查通过、CHANGELOG 已补、自审清单完成。
- 合并方式：**squash merge**，保持 `main` 线性；合并后删除远端与本地分支。
- 不在 PR 里混入无关格式化或重构。
- 单人项目没有外部 reviewer，但 PR 必须留下自审结论。

## 5. 常用命令

```bash
# 1. 建 issue（或用模板）
gh issue create --title "fix: ..." --body "..." --label bug --label p2

# 2. 分支 + 修改 + 本地验证
git switch -c fix/12-sql-hang
make clean && make -j"$(nproc)" && python3 scripts/check_docs.py
git add -A && git commit -m "fix(sql): 数据库连接失败时返回 500 而不是挂起"

# 3. 开 PR（正文必须 Closes #12）
git push -u origin fix/12-sql-hang
gh pr create --fill --body "Closes #12
..."
gh pr checks --watch

# 4. 合并并清理
gh pr merge --squash --delete-branch
git switch main && git pull --ff-only
```
