# conventions/

本目录是本仓库**强制遵守**的工程规范。它们从本机另一个成熟的多人协作项目（`admin`）
按需裁剪而来：保留可执行的约束，去掉个人学习项目用不上的仪式。

| 文档 | 什么时候读 |
|------|-----------|
| [PROJECT.md](./PROJECT.md) | 第一次参与；确认项目定位、边界与“完成”的定义 |
| [DOCUMENTATION.md](./DOCUMENTATION.md) | 新增/移动/修改任何文档，或改代码后判断该同步哪些说明 |
| [VERSIONING.md](./VERSIONING.md) | 改版本号、写 CHANGELOG、打 tag、准备发布之前 |
| [GIT-WORKFLOW.md](./GIT-WORKFLOW.md) | 开分支、写提交、建 issue、开 PR 之前 |
| [TESTING.md](./TESTING.md) | 写测试、报告验证结果、跑压测之前 |
| [CODE-REVIEW.md](./CODE-REVIEW.md) | 提交前自审，或 review 别人的 diff |

## 与其他文档的关系

- [AGENTS.md](../../AGENTS.md)：AI/协作者入口，规定读文档的顺序与硬性约束。
- [docs/INDEX.md](../INDEX.md)：文档总索引。
- [docs/architecture/](../architecture/ARCHITECTURE.md)：当前系统事实，不重复规范。
- [docs/history/iterations/](../history/iterations/README.md) 与 [docs/history/](../history/PROJECT-LOG.md)：
  历史材料，不作为现行操作依据。

## 规范的效力

1. 规范与当前代码冲突时，**先确认哪边是对的**：代码是事实来源，文档是承诺；两者不一致按
   [DOCUMENTATION.md](./DOCUMENTATION.md#过期即缺陷) 处理。
2. 规范可以被修改，但必须通过一次独立提交说明原因，并同步更新本索引。
3. 违反规范不涉及“惩罚”，但**不允许把不符合规范的产物合入 `main`**。
