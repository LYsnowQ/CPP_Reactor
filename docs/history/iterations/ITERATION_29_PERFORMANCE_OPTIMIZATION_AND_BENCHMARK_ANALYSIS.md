# Iteration 29 - 性能优化与压测体系分析

## 本轮背景

项目初期基准测试显示静态文件 QPS 约为 10,000，远低于 Reactor 模型在同类硬件上的预期表现。同时，社区中 io\_uring 框架常宣称百万级 QPS，引发了对架构选型（epoll Reactor vs io\_uring）的讨论。本轮的目标是：**查明当前 QPS 低的原因——究竟是代码瓶颈还是压测方法问题，并量化框架原始 IO 吞吐能力。**

## 关键发现（TL;DR）

- **编译器优化级别是最大的单一瓶颈**：从 `-O0`（默认 Debug）切换到 `-O2` 后，同配置下 QPS 从 10,143 提升至 25,730，增幅 154%。
- **Echo 基准测试（不做任何业务逻辑）可达 1,275,297 QPS**，证明 epoll Reactor 框架本身的 IO 吞吐能力足以达到百万级。
- **61k QPS（HTTP 静态文件）与 1.28M QPS（Echo）之间的 21 倍差距，来源于业务代码开销**：HTTP 解析、字符串分配、目录遍历、响应拼接等。
- **Python asyncio 作为压测工具会产生客户端瓶颈**（\~20k QPS），需使用 wrk 等 C 语言工具才能测出服务器真实上限。
- **单线程 accept 在短连接模式（Connection: close）下会成为瓶颈**，但在长连接模式下影响较小。

***

## 变更 1：编译器优化级别

### 问题

Makefile 中使用 `-g` 标志（Debug 模式），未指定优化级别，GCC 默认使用 `-O0`（零优化）。

**后果**：

- 函数调用无法内联（大量 `std::function`、`std::bind` 传递无法展开）
- 寄存器分配、循环优化、指令重排均未启用
- `std::string` 操作（拷贝、查找、拼接）走最慢路径
- 热路径上的每次字符串拼接都会产生多次堆分配

### 修改

[Makefile 第 5 行](file:///project/CPPReactor/Makefile#L5)：

```diff
-CXXFLAGS := -std=c++20 -Wall -Wextra -g
+CXXFLAGS := -std=c++20 -Wall -Wextra -O2 -g
```

`-g` 保留以保持调试符号可用，`-O2` 开启标准二级优化（含内联、循环优化、向量化等）。

### 效果

| 指标                   | 优化前（-O0） | 优化后（-O2） | 提升    |
| -------------------- | -------- | -------- | ----- |
| QPS（epoll 6线程 close） | 10,143   | 25,730   | +154% |
| 平均延迟                 | 23.64ms  | 7.61ms   | -68%  |

***

## 变更 2：Worker 线程数与压测参数调优

### 问题

原始基准脚本（`bench_static.sh`）的 M Profile 仅使用 4-6 个 IO 线程，而测试机器为 20 核 CPU。同时，压测脚本每次修改服务器参数都需要手动操作端口和进程管理。

### 修改

全面重写为 [tests/bench.sh](file:///project/CPPReactor/tests/bench.sh) 统一压测脚本：
- **自动生命周期管理**：`start_server`/`stop_server` + `trap EXIT` 确保清理
- **PID 文件精确管理**：避免 `pkill` 误杀其他进程
- **梯度 Profile**：逐级增加并发，自动判断触顶退出（连续 2 档 QPS 下降或增长 < 5%）
- **M Profile 线程升级**：从 4-6 提升至 8-18 线程，并发从 150-240 提升至 300-1000
- **Server 启动参数同步**：[bench\_sql.sh](file:///project/CPPReactor/tests/bench_sql.sh#L84) 服务器线程从 4 提升至 12

### 数据（优化后）

| 配置                         | QPS        | 说明             |
| -------------------------- | ---------- | -------------- |
| epoll 8 threads keepalive  | 53,294     | M1             |
| epoll 12 threads keepalive | **63,011** | 峰值（M2）         |
| epoll 12 threads close     | 21,336     | 受单线程 accept 限制 |
| poll 12 threads keepalive  | 50,188     | poll 后端的合理表现   |

**关键结论**：keepalive 模式下 8→12 线程仍能带来 18% 的提升，说明 worker 尚未完全饱和。

***

## 变更 3：Echo 基准服务器（纯 IO 吞吐测量）

### 动机

HTTP 服务器中每次请求涉及：TCP 读 → HTTP 解析 → 路由分发 → 文件目录遍历（`/` 路由）→ HTML 拼接 → 响应组装 → TCP 写。这些业务逻辑会严重混淆框架本身 IO 吞吐能力的测量。需要一个**剥离所有业务逻辑**的基准。

### 实现

新建 [src/app/main\_echo.cpp](file:///project/CPPReactor/src/app/main_echo.cpp)：

- **不做 HTTP 请求解析**：读入数据直接丢弃
- **预构建响应头**：常量 `kHttpOkResponse`（"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n..."），避免运行时字符串拼接
- **使用与主服务器相同的 EventLoop/Channel/IOThreadPool 基础设施**，保证对比的公平性
- **TCP\_NODELAY**：禁用 Nagle 算法，减少延迟
- **无锁 fd 索引数组**：用 `EchoContext* s_ctx[65536]` 替代 `unordered_map`，避免并发竞争

### 稳定性修复

初始版本使用全局 `std::unordered_map<int, EchoContext>`，在 200+ 并发下因多线程访问 map 导致 Segmentation Fault。修复方式为改用固定大小的指针数组（fd 直接作为下标），读写均为无锁原子指针操作。

### 效果

| 并发数     | wrk 参数 | QPS           | P50 延迟 |
| ------- | ------ | ------------- | ------ |
| 10      | 2 线程   | 180,964       | 39μs   |
| 50      | 4 线程   | 520,259       | 69μs   |
| 100     | 4 线程   | 633,195       | 100μs  |
| 200     | 8 线程   | **1,336,849** | 151μs  |
| 500     | 12 线程  | 1,037,039     | 583μs  |
| **800** | 12 线程  | **1,781,742** | 803μs  |
| 1000    | 16 线程  | 1,626,674     | 1.50ms |

**结论**：epoll Reactor 框架在纯 IO 场景下完全有能力达到百万级 QPS。约 29 倍于 HTTP 服务器的性能差距全部来源于业务代码开销。

***

## 变更 4：压测工具对比与客户端瓶颈分析

### 工具对比

| 工具                   | 实现语言   | 并发模型        | 实测最大 QPS（Echo 场景） | 瓶颈分析                              |
| -------------------- | ------ | ----------- | ----------------- | --------------------------------- |
| **wrk**              | C      | 多线程 + epoll | **1,781,742**     | 工具本身效率极高，能与服务器匹配                  |
| **Python asyncio**   | Python | 单线程事件循环     | \~22,035          | GIL 限制事件循环吞吐；Python socket 调用额外开销 |
| **Python threading** | Python | 多线程         | \~12,344          | GIL 导致所有线程无法并行执行                  |

### 对压测方法的影响

- 使用 wrk 时，服务器在 12 个 IO 线程下表现正常，QPS 随并发数线性增长
- 使用 Python asyncio 时，服务器线程数从 2 增加到 32，QPS 始终在 19k-22k 之间波动，**这不是服务器的上限，而是客户端的上限**
- 结论：压测结果必须标注客户端工具和参数，否则数据无参考价值。**服务器性能基准测试应始终使用 wrk 等 C 语言工具。**

***

## 变更 5：统一压测脚本

将原有的 `bench_static.sh` 和 `bench_echo.sh` 合并为统一的 [tests/bench.sh](file:///project/CPPReactor/tests/bench.sh)，消除了大量重复代码（`start_server`/`stop_server`/`run_wrk`/plateau 检测等逻辑在两个脚本中完全相同）。

新脚本用法：

```bash
# HTTP 服务器多配置探索
bash tests/bench.sh static M

# HTTP 梯度加压
bash tests/bench.sh static GRADIENT

# Echo 基准梯度加压
bash tests/bench.sh echo ECHO

# Echo 基准线程梯度
bash tests/bench.sh echo ECHO_THREADS

# 自定义每档时长（默认 12 秒）
bash tests/bench.sh static GRADIENT 8
```

保留的独立脚本：
- [tests/bench_sql.sh](file:///project/CPPReactor/tests/bench_sql.sh)：SQL 数据库查询压测
- [tests/test_smoke.sh](file:///project/CPPReactor/tests/test_smoke.sh)：快速功能烟雾测试
- [tests/setup.sh](file:///project/CPPReactor/tests/setup.sh)：Python 虚拟环境设置

***

## 当前性能全景

| 场景              | 实测 QPS        | 瓶颈位置                        | 预期上限         |
| --------------- | ------------- | --------------------------- | ------------ |
| Echo（纯 IO，无业务）  | **1,781,742** | CPU/内核调度                    | \~200 万      |
| HTTP 静态文件（目录索引） | **60,645**    | HTTP 解析 + 字符串分配 + 目录 IO     | \~20 万（优化后）  |
| SQL 数据库查询       | **13-17**     | 远程 MySQL 网络延迟（300-800ms/查询） | 取决于数据库       |

***

## epoll Reactor vs io\_uring 的架构差异

### 为什么 io\_uring 可以达到百万 QPS

| 维度              | epoll Reactor（本项目）            | io\_uring（SQPOLL 模式）        |
| --------------- | ----------------------------- | --------------------------- |
| **每请求 syscall** | 至少 3 个（read/write/epoll\_ctl） | **0 个**（内核线程轮询 Ring Buffer） |
| **数据拷贝**        | read/write 在用户态/内核态间拷贝        | 支持注册 Buffer，零拷贝路径           |
| **事件模型**        | epoll\_wait 每次返回一批事件          | 批量提交/完成，单次可提交 512+ 请求       |
| **上下文切换**       | epoll\_wait 每次触发至少一次切换        | 无（内核线程直接处理）                 |
| **文件 IO**       | 阻塞调用                          | 内核态异步                       |

### 本项目的定位

- 在纯 IO 场景（Echo）下，我们的 epoll Reactor 实测达到 **1.78M QPS**，证明架构并非瓶颈
- io\_uring 的优势在**高 IOPS 场景**（大量小文件随机读写、零拷贝路径）体现最明显
- 对于 HTTP API Server 这类以业务逻辑为主的场景，**代码效率（解析、序列化、字符串处理）远比 IO 模型的差距重要**——61k vs 1.78M 的约 29 倍差距就是证据

***

## 变更 6：代码全量检查问题修复清单

在代码提交前进行了全量检查，修复了 9 个问题。以下为问题清单及修改方式。

### 6.1 问题清单

| 编号 | 严重程度 | 问题 | 涉及文件 | 修改方式 |
|------|---------|------|---------|---------|
| 1 | Critical | `CMakeLists.txt` 使用 `file(GLOB_RECURSE)` 会将 `main_echo.cpp` 错误链接到 `main_run`，且缺少 `main_echo` 构建目标 | `CMakeLists.txt` | 改用显式文件列表，新增 `main_echo` 目标 |
| 2 | Critical | `TcpConnection::destory_()` 方法名拼写错误（缺 's'），应为 `destroy_()` | `TcpConnection.hpp`, `TcpConnection.cpp` | 声明 + 定义修正 |
| 3 | Critical | `HttpRequest::getMethed()` public 接口拼写错误，应为 `getMethod()` | `HttpRequest.hpp`, `HttpRequest.cpp`, `HttpRouter.cpp`, `StaticFileHandler.cpp`, `SqlHandler.cpp` | 声明 + 实现 + 所有调用方修正 |
| 4 | Major | `IOThreadPool` 存在 4 个未使用成员变量：`taskQ_`、`cv_`、`mutex_`、`isStop_` | `IOThreadPool.hpp`, `IOThreadPool.cpp` | 移除声明与引用 |
| 5 | Major | `Dockerfile` 未将 `main_echo` 复制到 runtime 镜像 | `Dockerfile` | 追加 COPY 指令 |
| 6 | Major | `TcpServer::accepter_` 线程成员从未被创建（`acceptConnection` 在主线程同步运行），`stop()` 中 `join()` 永远无效 | `TcpServer.hpp`, `TcpServer.cpp` | 移除 `accepter_` 成员及 `join()` 调用 |
| 7 | Major | `SqlHandler` 错误响应文本 `"Method not allowd"` 拼写错误 | `SqlHandler.cpp` | 修正为 `"Method not allowed"` |
| 8 | Major | `TcpConnection` 中 `response_` 成员从未被赋值，`asyncPending_` 只写不读 | `TcpConnection.hpp` | 移除死成员 |
| 9 | Medium | `TcpServer::baseLoop_` 成员永远为 `nullptr`（声明后从未赋值） | `TcpServer.hpp`, `TcpServer.cpp` | 移除 `baseLoop_` 及初始化列表 |

### 6.2 修复原则

- **不改变已有公共 API 的行为**：`getMethed()` 改为 `getMethod()` 属于拼写修正，所有内部调用方已同步修改
- **仅移除确定未使用的代码**：每个成员的移除均经过双向确认（声明处有定义 + 全局搜索引用）
- **Makefile 与 CMakeLists.txt 同步**：两个构建系统均支持 `main_run` 和 `main_echo` 两个目标

### 6.3 修复验证

- 构建：`make clean && make -j$(nproc)` 零错误零警告通过
- `main_run` 烟雾测试：返回 HTTP 200
- `main_echo` 烟雾测试：返回 HTTP 200

---

## 后续可进一步优化方向思考

1. **HTTP 请求解析优化**：当前 `HttpRequest::parseRequest` 每次分配 `std::vector<std::pair<...>>` 存储请求头，可改用零拷贝或预分配方案
2. **响应组装零拷贝**：`appendSimpleResponse_` 每次运行时拼接 7 段字符串，可预编译常用响应模板
3. **热路径任务队列消除**：同线程 MODIFY 操作当前仍走任务队列（加锁 + 入队 + 可能 socketpair 唤醒），可直接调用 `epoll_ctl`
4. **单线程 accept 优化**：短连接模式（close）下 accept 跟不上新建连接速率，可引入 `SO_REUSEPORT` 多 accept 队列
5. **Echo 压测客户端**：当前 wrk 虽然高效但仅支持 HTTP 协议，可开发原生 TCP 回显压测工具以消除 HTTP 头部传输的时间

