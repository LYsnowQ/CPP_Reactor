# Iteration 15 - HTTP 响应边界语义收敛

## 本轮目标
- 收敛简单响应的 HTTP 边界语义，降低压测端 `read errors`。
- 在不改动大架构前提下，先提升可用性和协议一致性。

## 技术点与代码位置
- 统一简单响应头，显式声明响应边界：
  - `TcpConnection.cpp`：`appendSimpleResponse_()`
  - 新增头字段：`Content-Length: 0`、`Connection: close`、`Server: CPPReactor`

## 为什么这样改
- 之前仅返回状态行 + 空行，客户端需依赖连接关闭推断消息边界。
- 显式 `Content-Length` 能减少客户端解析歧义，`Connection: close` 与当前“一请求一连接”行为保持一致。

## 前后对比（同量级 wrk）
- 修改前（18080）：`134014 requests / 8.07s`，`Socket errors: read 183221`
- 修改后（18081）：`516888 requests / 8.10s`，`Socket errors: read 92480`
- 结论：吞吐显著提升，`read errors` 绝对值下降且相对比例明显改善。

## 压测术语补充
- `Socket errors: read`：客户端读响应失败次数，通常与连接关闭时机、响应边界声明、协议一致性相关。
- `Transfer/sec`：每秒传输字节数，反映有效负载输出能力。

## 相比上一版本解决的问题
- 相比 Iteration 14，本轮从协议语义层面收敛了响应边界，减少了客户端解析不确定性。
- 为下一步“连接关闭时序优化（进一步压低 read errors）”打好基础。
