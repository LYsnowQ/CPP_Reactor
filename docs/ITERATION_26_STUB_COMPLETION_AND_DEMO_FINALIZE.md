# Iteration 26 - 空桩代码修缮、设计收敛与契约墙体系建设

## 本轮目标
- 将设计方案中的 3 处 TODO 空桩全部实施落地
- 清除项目中所有不必要的向后兼容层，简化架构
- 构建完整的 API 契约墙文档体系
- 确认 Demo 阶段竣工，输出项目整体现状与脉络清单

## 空桩落实详情

### 1. 设计 A：HttpRouter 路由分发器

全量实现路由分发组件，替代 `main.cpp` 中原有的硬编码 lambda 路由。

**新增文件**：
- [HttpRouter.hpp](file:///project/CPPReactor/include/protocol/HttpRouter.hpp) — 路由条目结构体 + HttpRouter 类定义
- [HttpRouter.cpp](file:///project/CPPReactor/src/protocol/HttpRouter.cpp) — 完整匹配逻辑

**功能特性**：

| 特性 | 使用方式 |
|------|---------|
| 方法路由 | `router.get("/query", handler)`、`router.post("/users", handler)` |
| 路径参数 | `router.get("/users/:id", handler)` → `req.getPathParam("id")` |
| 前缀匹配 | `router.addPrefix("/static/", handler)` |
| 自动 404 | 无匹配时返回 404 HandlerResult |
| 优先级 | 精确匹配 > 前缀匹配，同类别按注册顺序 |

**核心匹配算法**：
- `splitPath_` — 将 URL 按 `/` 拆段，自动去除 query string，合并连续斜杠
- `matchSegments_` — 逐段比较 pattern 与 URL，`:` 开头的段视为参数并提取
- `dispatch` — 两轮匹配（精确优先 → 前缀兜底），大小写不敏感方法校验

### 2. 设计 B：IHttpHandler 请求处理器抽象接口

定义处理器抽象基类，`StaticFileHandler` 和 `SqlHandler` 继承统一接口。

- [IHttpHandler.hpp](file:///project/CPPReactor/include/handler/IHttpHandler.hpp) — `handle()` 纯虚函数 + `name()` 虚方法
- [StaticFileHandler.hpp/.cpp](file:///project/CPPReactor/include/handler/StaticFileHandler.hpp) — 改为继承 IHttpHandler，`createHandler` 工厂替换为 public 构造
- [SqlHandler.hpp/.cpp](file:///project/CPPReactor/include/handler/SqlHandler.hpp) — 同上

### 3. 设计 C：SqlRepository 回调接线

**1 行代码修复**：[SqlRepository.cpp:39](file:///project/CPPReactor/src/repository/sql/SqlRepository.cpp#L39)

```cpp
// 之前
//TODO:执行回调

// 之后
done(std::move(res));
```

`findUserByIdAsync` 的异步查询结果现在通过回调正确返回给调用方。

## 设计收敛：移除不必要的向后兼容层

根据评估报告，项目尚未发布上线，保留向后兼容层只会增加不必要的复杂度。执行方案一，直接消除所有兼容层：

| 移除项 | 文件 | 原因 |
|--------|------|------|
| `StaticFileHandler::createHandler()` 工厂 | [StaticFileHandler.hpp](file:///project/CPPReactor/include/handler/StaticFileHandler.hpp) | 构造函数已 public，直接 `make_shared` |
| `SqlHandler::createHandler()` 工厂 | [SqlHandler.hpp](file:///project/CPPReactor/include/handler/SqlHandler.hpp) | 同上 |
| `FunctionHandler` 适配器类 | [IHttpHandler.hpp](file:///project/CPPReactor/include/handler/IHttpHandler.hpp) | 不再需要将 lambda 包装为 IHttpHandler |
| `IHttpHandler::onRegister/onUnregister` | [IHttpHandler.hpp](file:///project/CPPReactor/include/handler/IHttpHandler.hpp) | 当前无人使用，保持接口最小 |
| `HttpRouter::RequestHandler` 旧注册重载 | [HttpRouter.hpp/.cpp](file:///project/CPPReactor/include/protocol/HttpRouter.hpp) | 改为接受 `shared_ptr<IHttpHandler>` |
| 工厂函数中的 shared_ptr + lambda 两层间接 | [StaticFileHandler.cpp](file:///project/CPPReactor/src/handler/StaticFileHandler.cpp) / [SqlHandler.cpp](file:///project/CPPReactor/src/handler/SqlHandler.cpp) | 消除一次堆分配和函数调用包装 |

**调用方式变化**：

```cpp
// 之前（兼容层）
auto sqlHandler = SqlHandler::createHandler(executor);
auto staticHandler = StaticFileHandler::createHandler(root);

// 现在（直接）
auto sqlHandler = std::make_shared<SqlHandler>(executor);
auto staticHandler = std::make_shared<StaticFileHandler>(root);
```

## 契约墙文档体系建设

为全部 8 个模块编写 API 契约墙大纲：

| 模块 | 覆盖类/类型 |
|------|-------------|
| Core | EventLoop、Epoll/Poll/SelectDispatcher、Dispatcher 工厂、Buffer、CoreStatus |
| Handler | IHttpHandler、StaticFileHandler、SqlHandler |
| Net | Channel、IOThreadPool、TcpConnection、TcpServer |
| Observability | Metrics、MetricsSnapshot |
| Protocol | HttpRequest、HttpResponse、HttpRouter |
| Repository | SqlRepository、SqlTask |
| Utils | StringUtils、JsonConfigLoader |
| Persistence | ISqlConnection、MySqlConnection、ThreadLocalSingleConn、SqlExecutor、SqlTransaction、SqlConfig、SqlTypes |

每个文件的流程为：读取全部头文件 → 读取全部源文件 → 按 `@brief @param @retval @thread @throw @warning` 编写公开契约，按 `// ====` 段落标注算法说明，按 `// 原因` 标注实现注释。

## 相比上一版本解决的问题

- **3 处 TODO 空桩全部落地**：HttpRouter 路由分发器、IHttpHandler 统一接口、SqlRepository 回调接线
- **架构简化**：消除所有不必要的向后兼容层（工厂函数 + 适配器 + 间接调用）
- **编译验证**：`make -j$(nproc)` 零错误零警告，服务器启动正常运行
- **契约墙体系建立**：8 模块的 API 契约墙大纲完整覆盖，可直接指导代码注释编写

## TODO 存量检查

| 位置 | 状态 |
|------|------|
| `src/` 中所有 TODO/FIXME/HACK 标记 | **0 处** ✅ |
| `include/` 中所有 TODO/FIXME/HACK 标记 | **0 处** ✅ |
| `#if 0` 死代码块 | **0 处** ✅ |

---

# Demo 阶段终止声明

**CPPReactor Demo 阶段正式结束。**

项目完成了从 C 风格原型到 C++20 Reactor 模式服务器的完整演进。

---

## 项目整体现状

### 目录结构

```
CPPReactor/
├── include/                    # 所有头文件
│   ├── core/                   # 事件循环、IO 复用、缓冲区
│   ├── handler/                # 请求处理器接口与实现
│   ├── net/                    # TCP 服务端、连接、Channel
│   ├── observability/          # 运行时指标采集
│   ├── persistence/            # SQL 持久化（接口 + MySQL 实现）
│   ├── protocol/               # HTTP 协议（请求/响应/路由）
│   ├── repository/sql/         # 数据仓储层
│   └── utils/                  # 工具函数
├── src/                        # 所有源文件（与 include 镜像）
├── third_party/spdlog/         # spdlog 日志库
├── config/                     # 运行时配置（SQLConfig.json）
├── tests/                      # 测试与压测体系
├── docs/                       # 项目文档
├── CMakeLists.txt              # CMake 构建
└── Makefile                    # 快速构建
```

### 模块完成度矩阵

| 模块 | 完成度 | 核心内容 |
|------|--------|---------|
| Core 事件循环 | **100%** | EventLoop 主循环、3 种 Dispatcher、Buffer、状态码体系 |
| Net 网络层 | **100%** | TcpServer 接入、IOThreadPool 轮询、TcpConnection 生命周期、Keep-Alive、异步响应 |
| Protocol 协议 | **100%** | HttpRequest 增量解析（状态机）、HttpResponse 构建、HttpRouter 路由分发 |
| Handler 处理器 | **100%** | IHttpHandler 抽象接口、StaticFileHandler（目录索引+路径穿越防护）、SqlHandler（异步+JSON） |
| Persistence 持久化 | **100%** | ISqlConnection 接口、MySqlConnection（JDBC）、ThreadLocalSingleConn、SqlExecutor、SqlTransaction |
| Repository 仓储 | **100%** | SqlRepository、SqlTask 设计模型 |
| Observability 可观测性 | **100%** | Metrics 单例（原子计数器 + 快照 + 窗口计算） |
| Utils 工具 | **100%** | URL 编解码、JSON 配置加载 |

### 架构脉络

```
┌─────────────────────────────────────────────────────────┐
│                      main.cpp                            │
│  ┌────────────────────────────────────────────────────┐  │
│  │  HttpRouter                                          │  │
│  │    ├─ addPrefix("/", StaticFileHandler)              │  │
│  │    └─ get("/query", SqlHandler)                      │  │
│  └──────────┬───────────────────────────────────────────┘  │
│             │ dispatch(HttpRequest&, TcpConnection&)        │
│             ▼                                              │
│  ┌────────────────────────────────────────────────────┐  │
│  │  TcpServer                                          │  │
│  │    ├─ IOThreadPool (N × EventLoop)                  │  │
│  │    ├─ acceptConnection() → TcpConnection::create()  │  │
│  │    ├─ conns_ (shared_ptr map)                       │  │
│  │    └─ cleanupClosedConnections_()                   │  │
│  └────────────────────────────────────────────────────┘  │
│                           │                               │
│              ┌────────────┴────────────┐                  │
│              ▼                         ▼                  │
│  ┌──────────────────┐    ┌─────────────────────┐         │
│  │ EventLoop (N个)   │    │  SqlExecutor        │         │
│  │   ├─ Dispatcher  │    │    ├─ 后台工作线程   │         │
│  │   ├─ channelMap  │    │    └─ jobQueue       │         │
│  │   ├─ callbackQ   │    │         │            │         │
│  │   └─ post()      │    │         ▼            │         │
│  └──────────────────┘    │  ThreadLocalSingleConn│        │
│         │                │    ├─ ISqlConnection  │         │
│         ▼                │    └─ MySqlConnection │         │
│  TcpConnection           └─────────────────────┘         │
│    ├─ Channel(fd)                                         │
│    ├─ handleRead → HttpRequest::parseRequest               │
│    ├─ requestHandler_ → IHttpHandler::handle               │
│    ├─ handleWrite → appendSimpleResponse_                  │
│    └─ handleClose → EventLoop::addTask(DELETE)             │
└─────────────────────────────────────────────────────────┘
```

### 数据流路径

```
HTTP 请求
  → TcpServer::acceptConnection()     [主线程]
    → TcpConnection::create(fd, loop) [主线程]
    → conn->init()                    [主线程 → EventLoop::addTask(ADD)]
      → epoll_wait 返回               [Worker 线程]
      → EventLoop::active(fd, event)  [Worker 线程]
      → TcpConnection::handleRead()   [Worker 线程]
        → HttpRequest::parseRequest() [Worker 线程]
        → HttpRouter::dispatch()      [Worker 线程]
          → IHttpHandler::handle()   [Worker 线程]
            ├─ StaticFileHandler: 同步 → appendSimpleResponse_
            │                     → handleWrite() → writeFd()
            │                     → handleClose() 或 Keep-Alive 重置
            │
            └─ SqlHandler: 异步 → SqlExecutor::submit()
                             → [后台线程] db.query()
                             → EventLoop::post(callback)
                             → [Worker 线程] sendAsyncResponse()
                             → handleWrite() → writeFd()
```

### 线程模型

| 线程 | 数量 | 职责 |
|------|------|------|
| 主线程（accept 线程） | 1 | `acceptConnection` 循环、连接容器清理、指标日志 |
| Worker EventLoop 线程 | N（命令行指定，默认 4） | 各连接上的 handleRead/handleWrite/handleClose |
| SqlExecutor 后台线程 | 1 | SQL 查询执行（JDBC 调用） |

### 当前项目信息

| 项目 | 值 |
|------|-----|
| C++ 标准 | C++20 |
| 构建系统 | Makefile + CMakeLists.txt 双路径 |
| 依赖 | spdlog（日志）、nlohmann/json（JSON）、MySQL Connector/C++（JDBC） |
| 编译 | `make -j$(nproc)` 零错误零警告 |
| 启动 | `./main_run <port> <resource_path> [dispatcher] [threads] [conn_mode]` |
