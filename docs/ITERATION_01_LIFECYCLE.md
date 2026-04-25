# Iteration 01 - 连接生命周期闭环（增量记录）

## 本轮目标
- 建立连接对象的可观测状态，避免 `TcpServer` 长期持有失效连接。
- 建立 `TcpServer` 侧的连接回收机制，形成“关闭 -> 标记 -> 回收”的闭环。
- 在不推翻现有模型前提下，先做最小增量稳态改造。

## 技术点与代码位置

### 1. 连接状态跨线程可见
- 技术点：
  - 将连接状态改为原子枚举，支持 worker 线程更新、accept 线程读取。
  - 增加 `isDisconnected()` 作为服务器回收判定入口。
- 代码位置：
  - `TcpConnection.hpp`:
    - 成员 `std::atomic<State> state_`
    - 方法 `bool isDisconnected() const`
  - `TcpConnection.cpp`:
    - `isDisconnected()`

### 2. Channel 销毁回调与连接状态收敛
- 技术点：
  - `Channel` 从 `EventLoop` 中删除并析构后，回调连接对象，把状态收敛到 `kDisconnected`。
  - 避免仅依赖 `handleClose()`，确保最终状态由“真实销毁事件”驱动。
- 代码位置：
  - `TcpConnection.hpp`:
    - 方法 `void onChannelDestroyed_()`
  - `TcpConnection.cpp`:
    - `init_()` 中 `destroyCallback` 绑定 `onChannelDestroyed_()`
    - `onChannelDestroyed_()` 实现

### 3. 服务器连接回收机制
- 技术点：
  - `TcpServer` 新增 `cleanupClosedConnections_()`，扫描 `conns_` 并回收已断开连接。
  - `acceptConnection()` 循环内周期执行回收，退出前补一次收尾回收。
  - 使用 `connsMutex_` 保护 `conns_` 的插入/回收/清空，规避并发访问风险。
- 代码位置：
  - `TcpServer.hpp`:
    - `std::mutex connsMutex_`
    - `void cleanupClosedConnections_()`
  - `TcpServer.cpp`:
    - `acceptConnection()` 中调用 `cleanupClosedConnections_()`
    - `cleanupClosedConnections_()` 实现
    - `stop()` 中 `conns_.clear()` 加锁

### 4. 非阻塞 accept + 轻量轮询回收
- 技术点：
  - 监听 fd 设置为非阻塞，`EAGAIN/EWOULDBLOCK` 下短暂休眠后继续循环。
  - 目的：在空闲期也能周期回收已关闭连接，避免“必须有新连接才触发回收”。
- 代码位置：
  - `TcpServer.cpp`:
    - 构造函数中 `fcntl(lfd_, F_SETFL, flags | O_NONBLOCK)`
    - `acceptConnection()` 中 `EAGAIN/EWOULDBLOCK` 分支与 `sleep_for`

## 本轮收益
- 连接关闭后不再只停留在 EventLoop 侧，`TcpServer` 管理视图可以逐步收敛到真实存活连接。
- 为下一轮“协议增量解析状态码化（kAgain/kInvalid）”提供稳定连接管理基础。

## 已知边界（后续迭代处理）
- 当前回收仍是“轮询式”，后续可升级为事件通知式（关闭事件直接投递到服务器回收队列）。
- HTTP 业务路径仍是临时响应逻辑，尚未接入业务 handler 抽象。

## 相比上一版本解决的问题
- 基线阶段（无前置迭代）主要问题是连接“只创建不回收”，本轮首次建立了连接生命周期闭环。
- 明确了连接管理归属：EventLoop 负责事件驱动，TcpServer 负责连接容器回收，减少职责混叠。
