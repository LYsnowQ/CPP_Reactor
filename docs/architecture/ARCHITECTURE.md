# ARCHITECTURE.md — 当前系统事实

> 本文件描述**代码现在如何工作**，不是设计方案或历史。与实现不一致时以代码为准，并立即修正本文件。
> 最后核对：2026-11-06（commit `221df66`）。

## 1. 运行单元

| 产物 | 入口 | 用途 |
|------|------|------|
| `build/main_run` | `src/app/main.cpp` | HTTP 服务器（静态文件 + 健康检查 + SQL 演示） |
| `build/main_echo` | `src/app/main_echo.cpp` | 压测基准：固定 HTTP 200 空响应，不做解析 |
| Docker 镜像 | `Dockerfile` | 同时包含两个二进制，默认启动 `main_run` |

`main_run` 只依赖本机文件系统和可选的 MySQL；没有外部服务注册、没有配置文件热加载。

## 2. 分层与模块

```
app/            入口、参数解析、依赖装配
 ├─ protocol/   HTTP 报文、路由
 ├─ handler/    业务处理器（静态文件、SQL）
 ├─ persistence/ 数据库连接、配置、执行器
 ├─ repository/  查询封装（SqlRepository/SqlTask）
 ├─ net/         TCP 服务器、连接、线程池、Channel
 ├─ core/        EventLoop、Buffer、事件分发后端
 ├─ observability/ 指标
 └─ utils/       JSON 配置加载、字符串工具
```

| 模块 | 关键类 | 职责 | 线程归属 |
|------|--------|------|---------|
| core | `EventLoop` | 任务队列、`post()` 跨线程投递、`socketpair` 唤醒、分发循环 | 每 IO 线程一个 |
| core | `Dispatcher` + 三后端 | epoll/poll/select 的注册、修改、删除与等待 | EventLoop 线程 |
| core | `Buffer` | 读缓冲、查找、切片、回收 | 所属连接线程 |
| net | `TcpServer` | 非阻塞 accept、round-robin 分配连接、连接容器、关闭回收、指标日志 | 主线程 |
| net | `TcpConnection` | 读事件 → 解析 → 分发 → 写回、Keep-Alive 判定、异步响应 | 所属 EventLoop 线程 |
| net | `IOThreadPool` | 创建 N 个 EventLoop 并运行 | 启动时创建 |
| net | `Channel` | fd 与读/写/错误回调的绑定 | 所属 EventLoop 线程 |
| protocol | `HttpRequest` | 请求行/头/`Content-Length` Body 的增量解析（上限 1 MB） | 连接线程 |
| protocol | `HttpRouter` | 方法 + 精确路径 + `:param` + 前缀匹配，未匹配 404 | 连接线程 |
| handler | `StaticFileHandler` | GET 文件/目录索引、`/healthz`、路径穿越防护 | 连接线程 |
| handler | `SqlHandler` | `GET /query|/sql?sql=`（仅 `--enable-sql` 注册），提交后台执行，异步回写 | 连接线程 + SQL 线程 |
| persistence | `SqlExecutor` | 单工作线程 + 任务队列；job 异常由 `SqlHandler` 捕获并回写 500 | 后台线程 |
| persistence | `ThreadLocalSingleConn` | 每线程一个连接，按生命周期/空闲/ping 重建 | 使用线程 |
| persistence | `MySqlConnection` | JDBC Connector/C++ 连接、query、事务 | 使用线程 |
| observability | `Metrics` | 原子计数器与快照 | 任意线程 |

## 3. 线程模型

```
主线程
  └─ TcpServer::acceptConnection()
       ├─ accept → 选 EventLoop → TcpConnection::create/init
       └─ 每 5 秒输出窗口指标

IO 线程 × N（默认 4）
  └─ EventLoop::run()
       ├─ dispatcher->dispatch() → Channel 回调
       ├─ 任务队列（addTask / post）
       └─ 本线程连接读写

SQL 线程 × 1
  └─ SqlExecutor::run_()
       ├─ 从队列取 job
       ├─ ThreadLocalSingleConn::withConnection(...)
       └─ 完成后 loop->post() 回写连接
```

约束：

- `TcpConnection`/`Channel` 只能在所属 EventLoop 线程操作；
- SQL 结果必须经 `EventLoop::post()` 回到连接线程；
- `Metrics` 使用原子操作，可从任意线程调用；
- `TcpServer::conns_` 由互斥锁保护，关闭回调先入队、由主循环统一回收。

## 4. 请求生命周期

1. `Channel` 收到可读事件 → `TcpConnection::handleRead()`；
2. 读入 `Buffer`，调用 `HttpRequest::parseRequest()`：
   - `kAgain`：等待更多数据；超过 5 秒返回 408；
   - 非法：400；`Content-Length > 1 MB`：413；
   - `kOk`：得到完整请求；
3. `HttpRouter::dispatch()`：先精确匹配（支持 `:param`），再前缀匹配，未命中 404；
4. handler 返回 `HandlerResult`：
   - 同步：`TcpConnection` 组装响应并写回；
   - 异步（`async = true`）：`SqlHandler` 在 SQL 线程完成后调用 `sendAsyncResponse()`；
5. 根据 Keep-Alive 配置与请求头决定复用或关闭；
6. 关闭的连接进入回调队列，由主线程统一 `erase` 并关闭 fd。

## 5. 连接生命周期

```
create(fd) → init()（注册读事件、设置回调）
   → 读/写事件循环
   → 关闭条件（对端关闭/错误/请求数超限/空闲超时/短连接）
   → closeCallback → TcpServer::enqueueClosedConnection_()
   → 主线程 cleanupClosedConnections_() → conns_.erase + close(fd)
```

## 6. 扩展点

### 新增 HTTP 路由

```cpp
router.get("/api/hello", handler);      // 精确匹配
router.post("/api/users/:id", handler); // 路径参数，handler 内用 req.getPathParam("id")
router.addPrefix("/static", handler);   // 前缀匹配
```

### 新增处理器

实现 `reactor::handler::IHttpHandler::handle()`，在 `main.cpp` 中用 `shared_ptr` 注册。
处理器运行在连接所属 EventLoop 线程，耗时操作必须投递到其他线程。

### 新增分发后端

1. `include/core/` 与 `src/core/` 增加实现；
2. `DispatcherType` 枚举与 `dispatcherTypeFromString/ToString`；
3. `Dispatcher` 工厂；
4. `CMakeLists.txt` 的 `CORE_SOURCES` 显式加入新文件；
5. 更新 [CONFIGURATION.md](./CONFIGURATION.md) 的 `dispatcher` 取值。

### 新增 SQL 配置字段

1. `SqlConfig` 结构体默认值；
2. `from_json` / `to_json` / `validate`；
3. 在 `MySqlConnection` 或 `ThreadLocalSingleConn` 中**真正使用**，或明确标注未生效；
4. 更新 [CONFIGURATION.md](./CONFIGURATION.md) 的字段表。

## 7. 已知限制

| 限制 | 影响 |
|------|------|
| 不支持 `chunked`、HTTP/2、WebSocket | 只适合简单请求 |
| 响应体整体在内存中组装，静态文件整文件读入 | 大文件会占用内存，无 Range |
| 单 SQL 工作线程、线程本地单连接 | 吞吐受限，非连接池 |
| `/sql` 默认关闭；开启后无鉴权，`readOnly` 仅提供启发式语句护栏 | 仅限本机/隔离网络 |
| 无优雅停机 | Ctrl-C 直接终止 |
| 无单元测试 | 回归主要靠端到端用例 |
| 日志级别固定 `debug` | 生产噪声大，也不可配置 |

完整问题清单与优先级见 [docs/plan/BACKLOG.md](../plan/BACKLOG.md)。
