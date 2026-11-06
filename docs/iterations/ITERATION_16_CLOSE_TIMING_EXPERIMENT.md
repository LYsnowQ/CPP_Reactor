# Iteration 16 - 关闭时序实验与回退结论

## 本轮目标
- 继续降低压测 `Socket errors: read`，尝试优化连接关闭时序。

## 实验内容
- 在 `TcpConnection.cpp` 中实验“写完后半关闭（`shutdown(SHUT_WR)`）并等待 EOF 再回收”。
- 观测吞吐、`read errors`、`timeout` 三项指标变化。

## 实验结果
- `read errors` 有下降趋势，但同时出现 `timeout` 且压测总耗时被拉长，吞吐不稳定。
- 对当前阶段“可用性优先 + 稳定吞吐”目标，副作用大于收益。

## 决策与落地
- 本轮最终回退为上轮稳定策略：
  - 保留 Iteration 15 已收敛的响应边界语义（`Content-Length: 0`、`Connection: close`）。
  - 不引入长等待 EOF 的半关闭流程，避免新 `timeout` 风险。

## 代码位置
- 关闭路径核心仍在：
  - `TcpConnection.cpp`：`handleWrite()` / `handleClose()`
  - `TcpConnection.cpp`：`appendSimpleResponse_()`（继承 Iteration 15 的响应头语义）

## 相比上一版本解决的问题
- 相比 Iteration 15，本轮给出了一次可验证的“关闭时序实验”结论：该路径当前不适合作为默认方案。
- 明确了后续方向应优先做“协议层一致性与连接策略设计”，而不是直接延长连接尾部生命周期。
