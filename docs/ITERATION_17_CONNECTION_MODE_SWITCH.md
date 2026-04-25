# Iteration 17 - 连接策略开关（close / keepalive）

## 本轮目标
- 在不破坏当前稳定链路前提下，增加连接策略开关。
- 保持默认 `close`，并提供 `keepalive` 实验模式用于压测和后续演进。

## 技术点与代码位置
- 启动参数新增连接模式：
  - `main.cpp`：新增第 5 个可选参数 `conn_mode`（`close|keepalive`）
  - `main.cpp`：启动日志输出 `conn_mode`
- 服务层透传配置：
  - `TcpServer.hpp/.cpp`：构造函数新增 `keepAliveEnabled`，成员 `keepAliveEnabled_`
  - `TcpServer.cpp`：创建连接后调用 `connRaw->setKeepAliveEnabled(keepAliveEnabled_)`
- 连接层按请求头协商：
  - `TcpConnection.hpp`：新增 `setKeepAliveEnabled(bool)`
  - `TcpConnection.cpp`：新增 `shouldKeepAlive_()`，按 `HTTP/1.0/1.1 + Connection` 判定是否复用
  - `TcpConnection.cpp`：`appendSimpleResponse_()` 动态返回 `Connection: keep-alive|close`
  - `TcpConnection.cpp`：`handleWrite()` 在 keepalive 场景下不关闭连接，仅关闭写事件并回到读态

## 压测对比（同量级 8s / 100c / 4t）
- close 模式（`18090`）：
  - `356864 requests in 8.10s`
  - `Requests/sec: 44059.04`
  - `Socket errors: read 77200`
- keepalive 模式（`18091`）：
  - `2368630 requests in 8.01s`
  - `Requests/sec: 295734.40`
  - 未出现 socket errors 输出（可视为该轮为 0）

## 术语补充
- `keep-alive`：连接复用策略，同一 TCP 连接可承载多个请求，减少握手与关闭开销。
- `Requests/sec`：吞吐核心指标，表示每秒完成请求数。

## 相比上一版本解决的问题
- 相比 Iteration 16，本轮不再只在“关闭时序”里做微调，而是从协议策略层提供可切换模式。
- 在不影响默认稳定模式的前提下，验证了 keepalive 对吞吐和错误率的显著改善空间。
