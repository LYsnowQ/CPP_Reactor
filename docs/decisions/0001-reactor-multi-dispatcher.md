# 0001 — Reactor 多线程模型与多分发后端

- 状态：accepted
- 日期：2026-04（回溯整理于 2026-11-06）
- 相关：`src/core/EventLoop.cpp`、`src/core/*Dispatcher.cpp`、`src/net/TcpServer.cpp`

## 背景

项目目标是系统实践 C++ 网络编程，需要一个能覆盖“连接生命周期 + 事件分发 + 协议解析 +
跨线程回调”的最小框架，而不是只做 echo。可选模型包括：

1. 单线程 Reactor（select/poll）；
2. 多线程 Reactor（主线程 accept + 多 IO 线程）；
3. thread-per-connection；
4. io_uring / proactor。

## 决策

采用 **多线程 Reactor**：主线程负责 accept 与连接回收，每个 IO 线程运行一个 `EventLoop`，
连接按 round-robin 分配；事件分发抽象为 `Dispatcher` 接口，**默认 epoll**，同时实现 poll 与
select 以便对比与教学。

## 理由

- epoll 在 Linux 上是学习网络编程的行业基线；
- 多线程 Reactor 能真实暴露并发、生命周期、跨线程回调问题，这正是项目想练的部分；
- 保留 poll/select 让“后端差异”可被压测对比，而不是纸面理解；
- 抽象 `Dispatcher` 接口的成本低，扩展新后端只需工厂加一个分支。

## 后果

正面：

- 压测中 Echo 达到百万级 QPS，证明 IO 调度不是瓶颈；
- `post()` + `socketpair` 的跨线程模型可复用到 SQL 异步回写；
- 三种后端可运行时切换，便于对比。

负面：

- 每个连接的回调必须绑定到所属 EventLoop，编码约束强；
- 关闭/回收路径复杂，历史上出现过提前释放与崩溃（见 ITERATION 13/14）；
- 没有 io_uring，无法覆盖 Linux 新 API。

## 替代方案

| 方案 | 放弃原因 |
|------|---------|
| 单线程 Reactor | 无法练习多线程生命周期与跨线程回调 |
| thread-per-connection | 高并发下线程数不可控，不是本项目的学习重点 |
| io_uring | 与现有 epoll 抽象差异大，且内核/环境依赖高，留作未来方向 |

## 重新评估条件

- 需要支持非 Linux 平台；
- 需要验证 io_uring 或协程模型；
- 现有 Dispatcher 抽象无法容纳新后端（届时先改抽象，再新增 ADR）。
