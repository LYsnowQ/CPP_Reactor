# VERSIONING.md — 版本与发布

## 1. 版本号

采用语义化版本 `MAJOR.MINOR.PATCH`：

| 位 | 何时递增 | 示例 |
|----|---------|------|
| MAJOR | 不兼容变更：命令行参数语义、配置字段必填性、HTTP 接口契约 | 2.0.0 |
| MINOR | 向后兼容的新能力：新路由、新分发后端、新配置项 | 1.3.0 |
| PATCH | 向后兼容的修复：bug、构建、测试、纯文档/配置修正 | 1.2.1 |

规则：

- 版本号**不自动提升**，由一次明确的发版动作决定。
- 预发布版本只在确实需要外部试用时使用：`1.3.0-rc.1`，不得与正式版共用同一 tag。
- 当前版本：**1.2.0**（2026-06-19）。

## 2. 唯一事实来源

1. **git tag** 是“发布过什么”的最终依据；
2. **[CHANGELOG.md](../../CHANGELOG.md)** 是“每一版改了什么”的人类可读记录；
3. 代码中的版本号（`CMakeLists.txt` 的 `project(... VERSION ...)`）必须与最新正式 tag 一致；
4. `docs/history/PROJECT-LOG.md` 是历史流水，不再承担版本记录职责。

`package.json` 之类不存在；不要新增第二份版本文件。

## 3. 历史版本补录

仓库早期只写了版本号、没有打 tag。2026-11-06 已按提交补打**本地** annotated tag：

| tag | 提交 | 说明 |
|-----|------|------|
| `v1.0.0` | `77e1e44` | Demo 阶段收口：路由、契约注释、Docker |
| `v1.1.0` | `af11b30` | 第三方库自包含，移除 Conan |
| `v1.2.0` | `483b7b1` | `-O2`、Echo 基准、压测体系统一 |

- 0.x 阶段提交与版本号并非一一对应，**不补 tag**，避免制造错误的发布事实；
- 补打的 tag 未推送；推送与否由所有者决定（`git push origin v1.0.0 v1.1.0 v1.2.0`）。
- **不为历史提交补 issue**：追溯价值低于噪音成本。issue 从下一次非平凡改动开始使用。

## 4. CHANGELOG 格式

遵循 Keep a Changelog，固定分节：

```markdown
## [Unreleased]

### Added
### Changed
### Fixed
### Docs

## [1.2.0] - 2026-06-19
...
```

- 每个面向使用者的变更都要有条目；内部重构写一行即可。
- 条目以动词开头，说明“影响”而不是“改了哪个文件”。
- 发布时把 `[Unreleased]` 内容移到新版本标题下，并补日期与比较链接。
- 纯格式、纯注释改动可不写。

## 5. 发版流程

1. 确认 `main` 上 CI 通过、工作区干净；
2. 确认 [BACKLOG](../plan/BACKLOG.md) 中没有被本次版本声称修复的未完成项；
3. 更新 `CHANGELOG.md`：把 `[Unreleased]` 提升为 `[X.Y.Z] - YYYY-MM-DD`；
4. 同步 `CMakeLists.txt` 的 `project(... VERSION X.Y.Z)`；
5. 核对文档：README 的版本徽章/当前版本、`CONFIGURATION.md`、`HTTP-API.md`（若接口有变）；
6. 提交 `chore(release): vX.Y.Z`；
7. 打 annotated tag 并推送（需所有者确认）：
   ```bash
   git tag -a vX.Y.Z -m "vX.Y.Z"
   git push origin main vX.Y.Z
   ```
8. 在 GitHub 用 CHANGELOG 对应段落创建 Release。

**tag 不可移动、不可删除。** 发错版本时发布新版本并在 CHANGELOG 说明，不改历史 tag。

## 6. 兼容性承诺

- 命令行参数：新增参数只能追加在末尾，且必须有默认值；
- `SQLConfig.json`：新增字段应提供默认值；必填字段变更属于 MAJOR；
- HTTP 接口：`/healthz` 与静态文件的 200/403/404/405 语义保持稳定；`/sql` 是演示接口，
  不承诺稳定，但仍需在 CHANGELOG 标注变更。
