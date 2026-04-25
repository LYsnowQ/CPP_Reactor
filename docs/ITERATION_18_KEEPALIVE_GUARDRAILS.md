# Iteration 18 - keepalive 护栏（复用次数与空闲超时）

## 本轮目标
- 在 keepalive 模式下增加资源护栏，避免长连接无限占用。
- 保持默认 close 行为不变，避免影响当前稳定主链路。

## 技术点与代码位置
- 连接层新增 keepalive 护栏配置与状态：
  - `TcpConnection.hpp`：
    - `setKeepAlivePolicy(uint32_t maxRequests, uint32_t idleTimeoutMs)`
    - `shouldCloseForIdle(int64_t nowMs) const`
    - 新增成员：`maxKeepAliveRequests_`、`keepAliveIdleTimeoutMs_`、`servedRequests_`、`lastActivityMs_`
- 连接层执行策略：
  - `TcpConnection.cpp`：
    - `handleRead()` / `handleWrite()` 更新 `lastActivityMs_`
    - `handleWrite()` 在 keepalive 响应完成后递增 `servedRequests_`，超阈值主动关闭
    - `shouldCloseForIdle()` 按 `nowMs - lastActivityMs_` 判断空闲回收
- 服务层接入策略并巡检回收：
  - `TcpServer.hpp/.cpp`：
    - 构造参数新增 `keepAliveMaxRequests`、`keepAliveIdleTimeoutMs`
    - `acceptConnection()` 对新连接调用 `setKeepAlivePolicy(...)`
    - `cleanupClosedConnections_()` 在 keepalive 模式下触发 idle 回收
- 启动参数透传：
  - `main.cpp`：
    - 新增可选参数 `[keepalive_max_requests] [keepalive_idle_ms]`
    - 启动日志输出当前阈值

## 压测对比（8s / 100c / 4t）
- close（`18092`）：
  - `550630 requests in 8.10s`
  - `Requests/sec: 67982.45`
  - `Socket errors: read 106371`
- keepalive（`18093`, `max_requests=100`, `idle_ms=10000`）：
  - `4241930 requests in 8.10s`
  - `Requests/sec: 523691.31`
  - `Socket errors: read 43000`

## 术语补充
- `max_keepalive_requests`：单连接最多复用请求数，超过后主动关闭，防止“超级长连接”长期占资源。
- `keepalive_idle_ms`：连接空闲回收阈值，超过后触发关闭，防止僵尸连接占位。

## 相比上一版本解决的问题
- 相比 Iteration 17，本轮从“能复用”升级到“可控复用”。
- 在高吞吐优势保持的同时，补上了资源上限与空闲回收这两个工业化必需护栏。
