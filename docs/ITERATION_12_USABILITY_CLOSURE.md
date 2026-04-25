# Iteration 12 - 可用性优先：HTTP 请求/响应闭环修复

## 本轮目标
- 优先保证“简单 HTTP 请求与响应”可用。
- 修复 `连接建立但无响应写出` 的核心链路问题。
- 给出前后对比数据，证明修复有效。

## 改动前问题
- 压测结果显示 `0 requests in 20s`，服务端 `write_Bps=0`、`resp_2xx=0`。
- 说明连接已接入但响应没有完成写出，链路卡在“读后未真正切写”。

## 技术点与代码位置

### 1) 在回包前显式开启写事件
- 技术点：
  - 在请求解析成功后，先开启 `write event`，再 `MODIFY`。
  - 在错误响应排队时，同样开启 `write event`，保证 `handleWrite()` 有机会被调度。
- 代码位置：
  - `TcpConnection.cpp`
    - `handleRead()` 中解析成功分支
    - `queueSimpleResponse_()` 中响应排队分支

## 前后对比（同机实测）

### 修复前（用户实测）
- `wrk`：`0 requests in 20.02s, 0.00B read`
- 服务端窗口日志：常见 `write_Bps=0.00`、`resp_2xx=0`

### 修复后（本轮复测）
- 冒烟：`./scripts/test_smoke.sh` -> `PASS (200)`
- 协议用例：`4` 例通过 `3`（`valid_get/invalid_request_line/split_content_length_body` 通过）
- 压测（10s, c=100, t=4）：
  - `29 requests in 10.00s`
  - `Requests/sec: 2.90`
  - 已从 `0` 提升到非零吞吐
- 服务端窗口日志示例：
  - `qps=0.20`、`resp_2xx=1`、`write_Bps=3.80`

## 本轮结论
- 项目已经恢复“简单 HTTP 请求/响应”的基础可用性。
- 但当前仍属于“可用雏形”，距离稳定压测状态还有差距（吞吐低、socket write errors 偏高）。

## 已知问题（下一轮优先）
- `payload_too_large` 用例仍可能连接重置（未稳定返回 413）。
- `wrk` 结果里 `Socket errors: write` 数值偏高，提示连接关闭与写路径时序仍需收敛。

## 下一轮建议
1. 稳定 413 路径：保证超大包场景“先回响应再关连接”一致性。
2. 收敛写事件生命周期：减少无效写尝试，降低 `Socket errors: write`。
3. 固化回归门槛：把“smoke + http_cases + baseline”作为每轮必跑清单。

