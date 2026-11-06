# Iteration 14 - 延迟回收与生命周期收敛

## 本轮目标
- 将连接回收从“轮询状态扫描”改为“关闭事件驱动回收”。
- 进一步收敛 `TcpConnection` 生命周期，降低并发下提前释放风险。

## 技术点与代码位置
- `TcpConnection` 新增关闭回调注入接口：
  - `TcpConnection.hpp`：`setCloseCallback(std::function<void(int)>)`
  - `TcpConnection.cpp`：`onChannelDestroyed_()` 内触发 `closeCallback_(fd_)`
- `TcpServer` 增加待回收队列：
  - `TcpServer.hpp`：`pendingCloseFds_`、`pendingCloseMutex_`、`enqueueClosedConnection_()`
  - `TcpServer.cpp`：`cleanupClosedConnections_()` 改为“批量取出待回收 fd 后定向 erase”
- 接入顺序收敛（避免 fd 复用窗口）：
  - `TcpServer.cpp`：先 `conns_.emplace()` 成功，再 `conn->init()`
  - `TcpServer.cpp`：`connRaw->setCloseCallback(...)` 在 `init()` 前注入

## 设计变化说明
- 旧方案：`accept` 线程按 `isDisconnected()` 全量扫描 `conns_` 决定销毁时机。
- 新方案：由 `EventLoop` 线程在 `Channel` 销毁时上报 fd，`accept` 线程仅回收“明确关闭”的连接。
- 好处：减少状态轮询误判窗口，回收触发条件更精确，生命周期边界更清晰。

## 前后对比

### 修改前
- 连接回收依赖状态扫描，回收触发时机与真实销毁事件弱关联。
- 在并发和 fd 复用场景中，存在生命周期竞争窗口。

### 修改后
- 回收来源从“扫描推断”变为“关闭事件通知”，回收路径单向明确。
- 压测下服务进程可持续运行，未复现此前的 139 崩溃路径。

## 压测术语补充
- `Socket errors: read`：压测端读响应时失败次数。常见于连接被对端提前关闭、响应语义与客户端预期不一致等场景。
- `Requests/sec`：每秒完成请求数（吞吐能力核心指标）。
- `Latency`：请求时延分布（服务响应快慢与稳定性）。

## 相比上一版本解决的问题
- 相比 Iteration 13，本轮将“连接回收触发条件”从被动扫描升级为事件驱动。
- 进一步降低 `TcpConnection` 提前释放的概率，为后续收敛 `read errors` 提供更稳定基础。
