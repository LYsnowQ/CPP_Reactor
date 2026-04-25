# Iteration 13 - 异常链路排查（Segmentation fault）

## 本轮目标
- 排查并修复压测期间出现的 `Segmentation fault`。
- 明确崩溃链路，给出前后对比和后续方向。

## 现象复盘
- 压测窗口内连接快速上升（`active_conn` 高），随后进程崩溃。
- 崩溃前常出现“读有增量、写为 0 或极低”的指标组合。

## 根因定位
- 根因类型：`use-after-free`（悬空指针访问）。
- 触发链路：
  1. `EventLoop::active()` 先取到 `Channel* channel`。
  2. `readFunc()` 回调中可能触发 `DELETE` 任务并在同线程立即处理，导致该 `Channel` 被删除。
  3. 回到 `active()` 后继续使用旧 `channel` 指针走写分支，产生悬空访问并崩溃。
- 关键位置：
  - `EventLoop.cpp`：`active()` 的读后写流程。

## 修复内容
- 在 `readFunc()` 返回后，重新检查 `channelMap_` 中该 `fd` 是否仍存在。
- 若已被删除，直接返回，不再访问旧指针。
- 代码位置：
  - `EventLoop.cpp`：`active()`（读回调后 re-check 逻辑）

## 前后对比

### 修复前
- 压测过程中可复现 `Segmentation fault (core dumped)`。
- 崩溃与高并发事件回调时序相关，稳定性不可接受。

### 修复后
- 本轮复测未复现该崩溃路径。
- 构建与基础回归通过，可继续推进可用性优化。

## 仍需继续关注
- `wrk` 仍存在较高 `Socket errors: write`，说明写路径时序仍有优化空间。
- 超大包场景（413）稳定性仍需单独收敛。

## 相比上一版本解决的问题
- 相比 Iteration 12，本轮解决了“运行中随机崩溃”的核心稳定性风险。
- 将问题从“不可控崩溃”降级到“可继续优化的性能/时序问题”。

