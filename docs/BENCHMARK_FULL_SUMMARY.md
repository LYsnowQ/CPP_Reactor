# CPPReactor 压测总结报告（独立文档）

> 说明：本文件为单独压测总结，不属于迭代增量记录。

## 1. 测试目标

- 对比不同 `dispatcher` 模型（`epoll/poll/select`）的吞吐、时延和错误状态。
- 观察线程数、并发连接数、压测时长、连接模式（`keepalive/close`）对性能的影响。
- 给出当前阶段可用推荐配置与后续优化方向。

## 2. 测试环境与方法

- 项目路径：`/project/CPPReactor`
- 压测工具：`wrk`
- 服务启动参数模板：
  - `./main_run <port> /project/CPPReactor <dispatcher> <io_threads> <conn_mode> 100 10000`
- 指标解释：
  - `requests`：总请求数
  - `req_per_sec`：每秒请求数（吞吐）
  - `latency_avg`：平均延迟
  - `transfer_sec`：每秒传输速率
  - `socket_errors`：客户端统计的连接/读写/超时错误

## 3. 实验表一：模型横向对比（8s）

| case | dispatcher | io_threads | conn_mode | wrk_threads | connections | duration | requests | req_per_sec | latency_avg | transfer_sec | socket_errors |
|---|---:|---:|---|---:|---:|---|---:|---:|---|---|---|
| E1 | epoll | 1 | keepalive | 2 | 50 | 8s | 55074 | 4986.07 | 7.34ms | 4.64MB | connect 0, read 540, write 0, timeout 50 |
| E2 | epoll | 4 | keepalive | 4 | 100 | 8s | 195390 | 24123.92 | 4.14ms | 22.43MB | connect 0, read 1904, write 0, timeout 0 |
| E3 | epoll | 8 | keepalive | 4 | 200 | 8s | 269942 | 33423.69 | 5.87ms | 31.08MB | connect 0, read 2610, write 0, timeout 0 |
| E4 | epoll | 8 | close | 4 | 200 | 8s | 163245 | 14765.38 | 6.80ms | 13.66MB | connect 0, read 5900, write 0, timeout 54 |
| P1 | poll | 4 | keepalive | 4 | 100 | 8s | 159308 | 19668.87 | 5.06ms | 18.29MB | connect 0, read 1544, write 0, timeout 0 |
| P2 | poll | 8 | keepalive | 4 | 200 | 8s | 207746 | 25816.29 | 7.65ms | 24.00MB | connect 0, read 1988, write 0, timeout 0 |
| S1 | select | 2 | keepalive | 2 | 64 | 8s | 103 | 12.72 | 3.55ms | 11.91KB | connect 0, read 1, write 0, timeout 0 |
| S2 | select | 4 | keepalive | 2 | 96 | 8s | 237 | 21.52 | 10.43ms | 19.35KB | - |

## 4. 实验表二：梯度实验（epoll + keepalive + io_threads=8）

| case | dispatcher | io_threads | conn_mode | wrk_threads | connections | duration | requests | req_per_sec | latency_avg | transfer_sec | socket_errors |
|---|---:|---:|---|---:|---:|---|---:|---:|---|---|---|
| G1 | epoll | 8 | keepalive | 2 | 50 | 5s | 106408 | 21277.15 | 2.43ms | 19.78MB | connect 0, read 1045, write 0, timeout 0 |
| G2 | epoll | 8 | keepalive | 4 | 100 | 8s | 155433 | 19192.61 | 5.27ms | 17.85MB | connect 0, read 1509, write 0, timeout 0 |
| G3 | epoll | 8 | keepalive | 4 | 200 | 8s | 182294 | 14748.22 | 8.67ms | 13.71MB | connect 0, read 1733, write 0, timeout 200 |
| G4 | epoll | 8 | keepalive | 8 | 400 | 8s | 183340 | 22634.60 | 17.27ms | 21.05MB | connect 0, read 1601, write 0, timeout 0 |
| G5 | epoll | 8 | keepalive | 4 | 200 | 20s | 411047 | 17914.96 | 9.76ms | 16.66MB | connect 0, read 4030, write 0, timeout 200 |

## 5. 结果解读

- 当前最强模型是 `epoll`，在 `keepalive` 模式下吞吐显著领先。
- `keepalive` 对吞吐提升明显（例如 `E3` 对比 `E4`），同时减少连接重建成本。
- `poll` 可用但整体性能低于 `epoll`，高并发下平均延迟更高。
- `select` 在当前压力条件下吞吐极低，建议仅保留兼容/教学用途。
- 现阶段主要问题已从“崩溃”转为“读错误偏高（read errors）”，属于可优化项而非阻断项。

## 6. 当前推荐配置（用于演示与基础可用）

- `dispatcher=epoll`
- `io_threads=4~8`（机器核数足够时偏向 8）
- `conn_mode=keepalive`
- 演示压测建议：
  - 中等压力：`wrk -t4 -c100 -d8s`
  - 较高压力：`wrk -t4 -c200 -d8s`

## 7. 后续优化方向

- 优先收敛 `read errors`：
  - 连接关闭时序细化（写完后读侧状态收敛）
  - keepalive 边界一致性检查
  - 超时参数按场景分层（空闲超时/请求超时）
- 增加稳定性长压：
  - `20s~60s` 连续压测，观察吞吐波动和错误累积
- 增加 P95/P99 统计展示（当前以平均延迟为主）

