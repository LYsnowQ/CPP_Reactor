# Iteration 25 - 死代码清理与空桩设计方案

## 本轮目标
- 清理项目中残留的旧版 C 风格死代码（`#if 0` 块与注释代码），减少代码膨胀与维护负担。
- 对剩余的 3 处 TODO 空桩代码进行设计分析，输出可实施方案供评估。

## 清理详情

### 1. `#if 0` 死代码块移除

| 文件 | 移除行数 | 内容说明 |
|------|---------|---------|
| [HttpRequest.cpp](file:///project/CPPReactor/src/protocol/HttpRequest.cpp) | ~430 行 | 旧 C 风格 HTTP 解析全过程：`httpRequestInit`、`parseHttpRequestLine`、`parseHttpRequestHead`、`parseHttpRequest`、`processHttpRequest`、`sendFile`、`sendDir`、`decodeMsg`、`getFileType` 等 |
| [HttpResponse.cpp](file:///project/CPPReactor/src/protocol/HttpResponse.cpp) | ~66 行 | 旧 C 风格响应处理：`httpResponseInit`、`httpResponseDestory`、`HttpResponseAddHeader`、`httpResponsePrepareMsg` |
| [TcpConnection.cpp](file:///project/CPPReactor/src/net/TcpConnection.cpp) | ~105 行 | 旧 C 风格连接管理：`tcpConnectionInit`、`processWrite`、`processRead`、`tcpConnectionDestory` |
| [HttpRequest.hpp](file:///project/CPPReactor/include/protocol/HttpRequest.hpp) | ~35 行 | 旧 API 声明 |
| [HttpResponse.hpp](file:///project/CPPReactor/include/protocol/HttpResponse.hpp) | ~9 行 | 旧 API 声明 |

### 2. 注释代码清理

| 文件 | 内容说明 |
|------|---------|
| [HttpRequest.cpp](file:///project/CPPReactor/src/protocol/HttpRequest.cpp) | 注释掉的 `getState()` 方法 |
| [IOThreadPool.cpp](file:///project/CPPReactor/src/net/IOThreadPool.cpp) | 注释掉的 `getLoop()` 方法及"暂时不实现"说明 |
| [TcpServer.cpp](file:///project/CPPReactor/src/net/TcpServer.cpp) | 注释掉的线程模式选择代码（kChiledThreadMode/kMianThreadMode） |
| [main.cpp](file:///project/CPPReactor/src/app/main.cpp) | 注释掉的旧静态文件服务器启动代码 |
| [MySqlConnection.cpp](file:///project/CPPReactor/src/persistence/MySqlConnection.cpp) | 注释掉的旧 URL 连接方式代码 |

### 清理原则
- 被移除代码均处于 `#if 0` 或完整注释状态，原本不会参与编译。
- 不影响现有功能、编译和运行时行为。
- 通过编译验证：

```bash
mkdir -p build-check && cd build-check && cmake .. && make -j$(nproc)
```

## 剩余 3 处 TODO 空桩

| 编号 | 位置 | 类型 | 描述 |
|------|------|------|------|
| A | [HttpRouter.hpp](file:///project/CPPReactor/include/protocol/HttpRouter.hpp#L1-L6) | 空壳文件 | 路由分发器仅空 namespace + TODO 注释 |
| B | [IHttpHandler.hpp](file:///project/CPPReactor/include/handler/IHttpHandler.hpp#L1-L6) | 空壳文件 | 请求处理器接口仅空 namespace + TODO 注释 |
| C | [SqlRepository.cpp](file:///project/CPPReactor/src/repository/sql/SqlRepository.cpp#L39) | 功能缺失 | `findUserByIdAsync` 中 SQL 查询结果已取出但回调 `done(res)` 从未被调用 |

## 相比上一版本解决的问题
- **代码库清理**：累计移除约 650 行死代码，降低代码膨胀和维护认知负担
- **风险消除**：移除可能误导开发者的旧代码遗留（如旧 API 声明在头文件中暴露）
- **方案明确**：3 处 TODO 均有完整设计方案，可评估后实施
