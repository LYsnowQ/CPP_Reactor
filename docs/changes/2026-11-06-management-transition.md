# 2026-11-06 管理转型：issue-first + PR + GitHub Issues 主档

- 类型：docs / chore
- 关联：[#18](https://github.com/LYsnowQ/CPP_Reactor/issues/18)
- 基线：`main` @ `2151180`

## 背景

项目此前是“直接提交 `main` + 本地 BACKLOG”的学习项目管理方式。规范化之后，代码、测试和文档
已经具备可追踪的基础，但待办仍分散在 `docs/plan/BACKLOG.md`，与 GitHub Issues 重复；
合并也没有走 PR。目标是逐步转为正常项目的管理方式，同时不给单人维护增加过重仪式。

## 本次变更

1. **GitHub Issues 成为唯一待办主档**：
   - 剩余 B-05–B-12 与新发现的 N-01–N-04 已全部建单；
   - `docs/plan/BACKLOG.md` 改为迁移说明与 issue 索引，删除逐条详情；
   - 关闭的问题不再在本地保留记录。
2. **PR 流程**：
   - 非平凡改动必须 issue → 分支 → PR → squash → 删除分支；
   - PR 正文必须 `Closes #N`，合并后自动关闭 issue；
   - 直接提交 `main` 仅保留 typo、单篇文档修正、发布提交；
   - 更新 `docs/conventions/GIT-WORKFLOW.md` 与命令示例。
3. **登记新问题**：
   - N-01：只读 SQL 护栏可被 `SELECT ... INTO OUTFILE` / `FOR UPDATE` 等绕过（p1）；
   - N-02：`/sql` 使用 GET 传递 SQL（p2）；
   - N-03：缺少 SECURITY.md 与贡献者 DCO/CLA（p2）；
   - N-04：CI checkout 过旧、`main` 缺少分支保护（p2）。

## 关于 #1–#5

`#1`–`#5` 在 PR 流程启用前已直接 fast-forward 合并到 `main`，提交已在主线上：

- 无法再创建有意义的 PR（base 已包含 head）；
- 不为它们重写历史或伪造 PR；
- 从 `#18` 起全部走 PR，作为流程转折点。

## 验收

- [x] 12 个剩余/新问题已建 issue（#6–#17）；
- [x] BACKLOG 不再复制 issue 详情；
- [x] GIT-WORKFLOW 明确 PR 必需、squash、删除分支；
- [x] `python3 scripts/check_docs.py` 通过；
- [x] 本 issue 通过 PR 合并并自动关闭。

## 后续

- #17 完成 `main` 分支保护与 CI checkout 升级；
- 后续每个 issue 一个分支、一个 PR；
- 发布流程见 [VERSIONING](../conventions/VERSIONING.md)。
