# Iteration 22 - 异步 SQL 处理器与跨线程回调机制

## 本轮目标
- 实现 `EventLoop::post()` 跨线程回调投递机制，使 SQL 后台查询结果能安全回写到 Reactor 主线程。
- 实现 `SqlHandler` HTTP SQL 查询处理器，通过 URL 查询参数提交 SQL 并异步返回 JSON 结果。
- 调整 `TcpConnection` 生命周期为 `shared_ptr`，适配异步响应场景下连接对象跨线程引用安全。
- 将 URL 编解码工具函数从 `StaticFileHandler` 提取到公共工具模块。

## 技术点与代码位置

### 1. 跨线程回调投递（EventLoop::post）
- [EventLoop.hpp](file:///project/CPPReactor/include/core/EventLoop.hpp#L43-L44)：新增 `Callback` 类型别名与 `callbackQueue_` 队列
- [EventLoop.hpp](file:///project/CPPReactor/include/core/EventLoop.hpp#L59)：新增 `post(Callback)` 方法声明
- [EventLoop.cpp](file:///project/CPPReactor/src/core/EventLoop.cpp#L252-L263)：`run()` 循环中每轮 epoll_wait 结束后批量处理回调队列
- [EventLoop.cpp](file:///project/CPPReactor/src/core/EventLoop.cpp#L265-L278)：`post()` 实现——加锁入队 + 跨线程时通过 `socketpair` 唤醒目标 EventLoop

### 2. 异步响应支持
- [TcpConnection.hpp](file:///project/CPPReactor/include/net/TcpConnection.hpp#L17)：继承 `std::enable_shared_from_this<TcpConnection>`
- [TcpConnection.hpp](file:///project/CPPReactor/include/net/TcpConnection.hpp#L27)：`HandlerResult` 新增 `async` 标记字段
- [TcpConnection.hpp](file:///project/CPPReactor/include/net/TcpConnection.hpp#L30)：`RequestHandler` 签名改为接收连接引用 `(HttpRequest&, TcpConnection&)`
- [TcpConnection.hpp](file:///project/CPPReactor/include/net/TcpConnection.hpp#L52-L53)：新增 `getLoop()` 与 `sendAsyncResponse()` 方法
- [TcpConnection.hpp](file:///project/CPPReactor/include/net/TcpConnection.hpp#L89)：新增 `asyncPending_` 状态标记
- [TcpConnection.cpp](file:///project/CPPReactor/src/net/TcpConnection.cpp#L61)：`create()` 返回 `shared_ptr`
- [TcpConnection.cpp](file:///project/CPPReactor/src/net/TcpConnection.cpp#L224-L231)：处理器返回 `async=true` 时跳过同步写路径
- [TcpConnection.cpp](file:///project/CPPReactor/src/net/TcpConnection.cpp#L383-L424)：`sendAsyncResponse()` 实现——填充响应字段、关闭异步标记、触发写事件

### 3. 连接生命周期 shared_ptr 化
- [TcpServer.hpp](file:///project/CPPReactor/include/net/TcpServer.hpp#L57)：`conns_` 从 `unique_ptr` 改为 `shared_ptr`

### 4. SqlHandler 实现
- [SqlHandler.hpp](file:///project/CPPReactor/include/handler/SqlHandler.hpp#L1-L15)：`SqlHandler::createHandler` 接口定义
- [SqlHandler.cpp](file:///project/CPPReactor/src/handler/SqlHandler.cpp#L1-L175)：完整实现，核心流程：
  - `extractSqlParam_()`：从 URL 查询字符串提取 `sql` 参数
  - `formatRowsToJson_()`：将 `SqlRows` 行集格式化为 JSON 数组
  - `createHandler()` 返回 `RequestHandler`，处理器标记 `async=true`，通过 `SqlExecutor::submit()` 投递到后台线程执行，结果通过 `EventLoop::post()` 投递回主线程，最后调用 `sendAsyncResponse()` 写回

### 5. StringUtils 公共工具模块
- [StringUtils.hpp](file:///project/CPPReactor/include/utils/StringUtils.hpp#L1-L73)：提取 `hexValue()`、`urlDecode()`、`urlEncodePathComponent()` 三个工具函数
- [StaticFileHandler.cpp](file:///project/CPPReactor/src/handler/StaticFileHandler.cpp#L238-L243)：移除内联 URL 编解码函数，改用 `utils::StringUtils` 版本

### 6. ClangD 独立编译配置
- [.clangd](file:///project/CPPReactor/.clangd)：从 CMake CompilationDatabase 依赖切换为手动指定 `-std=c++20` 和头文件搜索路径

### 7. SQL 连接配置
- [SQLConfig.json](file:///project/CPPReactor/config/SQLConfig.json)：JSON 配置文件，包含连接参数、超时规则、连接模式、观测与保护配置

## 异步执行流
```
HTTP 请求到达 → TcpConnection::handleRead()
  → 解析 HttpRequest
  → 调用 RequestHandler(req, conn) 
      → SqlHandler 提取 sql 参数，标记 result.async = true，return
  → TcpConnection 检测 async = true，跳过同步写
  → SqlExecutor::submit() 投递到后台线程
      → 后台线程：通过 ThreadLocalSingleConn 执行 SQL 查询
      → 查询完成后：EventLoop::post(callback) 投递回主线程
          → 回调中：conn->sendAsyncResponse(resp) 写回 HTTP 响应
```

## 相比上一版本解决的问题
- 相比 Iteration 21，本轮实现了：
  1. **SQL 查询异步化闭环**：从 HTTP 请求到 SQL 执行到结果回写，全链路异步打通
  2. **跨线程回调安全**：`EventLoop::post()` 配合 `shared_ptr` 确保连接对象在跨线程场景下不被提前销毁
  3. **工具函数复用**：URL 编解码从内联实现提取为公共模块，降低重复代码维护成本
  4. **构建独立性提升**：clangd 不再依赖 CMake Build 产物即可正确索引
