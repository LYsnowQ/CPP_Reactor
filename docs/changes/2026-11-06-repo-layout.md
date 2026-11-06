# 2026-11-06 根目录与 docs 结构整理

- 类型：docs
- 关联：issue [#24](https://github.com/LYsnowQ/CPP_Reactor/issues/24)
- 基线：`main` @ `775a148`

## 评估：docs 是否需要上传

**需要，继续随仓库版本化。** 理由：

- README、AGENTS、架构/接口/决策文档互相引用，删除或移出仓库会破坏可追溯性；
- 审计、变更记录和 ADR 是项目历史的一部分，适合公开保存；
- 不需要额外发布 GitHub Pages：GitHub 已能渲染 Markdown，Pages 只会增加部署面与重复导航。

## 根目录标准

整理后根目录只保留：

| 类别 | 文件 |
|------|------|
| 入口 | `README.md`、`README.en.md`、`AGENTS.md`、`CHANGELOG.md` |
| 许可 | `LICENSE` |
| 构建 | `CMakeLists.txt`、`Makefile`、`compile_flags.txt` |
| 容器 | `Dockerfile`、`docker-compose.yml`、`docker-entrypoint.sh` |
| 工具/忽略 | `.clang-format`、`.clangd`、`.gitignore`、`.dockerignore` |

移出根目录的非标准文件：

| 旧位置 | 新位置 | 原因 |
|--------|--------|------|
| `LICENSING.md` | `docs/legal/LICENSING.md` | 说明性文档，不是许可证正文 |
| `LICENSING.en.md` | `docs/legal/LICENSING.en.md` | 同上 |
| `LICENSE-MIT` | `LICENSES/MIT.txt` | SPDX 风格的历史许可证目录 |

## docs 分层

- 现行文档：`conventions/`、`architecture/`、`decisions/`、`legal/`、`plan/`、`reviews/`、`changes/`；
- 历史材料：`history/`（`PROJECT-LOG.md` + `iterations/` 29 篇），显式“仅供追溯”；
- 总入口：`docs/INDEX.md`。

`docs/iterations/` 整体迁入 `docs/history/iterations/`，与现行架构文档分开，避免读者把历史结论
当成当前行为。

## 链接修正

同步更新了：

- `README.md`、`README.en.md` 的徽章、文档导航、项目结构、许可段落；
- `docs/INDEX.md` 的根目录、`legal/`、`history/` 分组；
- `AGENTS.md`、`docs/conventions/DOCUMENTATION.md`、`docs/conventions/README.md`；
- ADR-0004、许可变更记录、发布记录、英文文档记录、规范化审计中的路径。

## 验证

- [x] `python3 scripts/check_docs.py` 通过；
- [x] 根目录不再有 `LICENSING*` / `LICENSE-MIT`；
- [x] 历史迭代只存在于 `docs/history/iterations/`；
- [x] 通过 PR 合并并删除分支。
