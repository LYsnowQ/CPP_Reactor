# decisions/

架构决策记录（ADR）。每条记录回答“为什么这样选、放弃了什么、什么条件下重新评估”。

| 编号 | 决策 | 状态 |
|------|------|------|
| [0001](./0001-reactor-multi-dispatcher.md) | 采用 Reactor 多线程 + epoll 默认，保留 poll/select 后端 | accepted |
| [0002](./0002-vendored-third-party.md) | 第三方库自包含于 `third_party/`，不使用 Conan/系统包 | accepted |
| [0003](./0003-sql-endpoint-demo-only.md) | `/sql` 仅作为演示端点保留，不承诺安全与稳定 | accepted（带技术债） |

## 约定

- 状态取值：`proposed`（提议）、`accepted`（已接受）、`superseded`（被取代）、`rejected`（否决）。
- 已接受的决策不直接改写结论；需要变更时新建一条并标注 `supersedes`。
- 决策不是操作手册：具体配置与接口仍以 `docs/architecture/` 为准。
- 新增决策后更新本索引与 [docs/INDEX.md](../INDEX.md)。
