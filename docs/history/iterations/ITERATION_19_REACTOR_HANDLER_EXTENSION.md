# Iteration 19 - Reactor 业务处理器扩展点

## 本轮目标
- 完成“基本可用框架”形态：核心仍是 Reactor，业务逻辑可插拔。
- 让后续 HTTP/SQL/Redis 第三方库以“平替处理器”方式接入，而不是改动事件循环主链路。

## 技术点与代码位置
- 连接层新增业务处理器接口：
  - `TcpConnection.hpp`
    - `HandlerResult`：业务处理结果（`statusCode`、`reasonPhrase`、`closeConnection`）
    - `RequestHandler`：`std::function<HandlerResult(HttpRequest&)>`
    - `setRequestHandler(...)`
  - `TcpConnection.cpp`
    - `handleRead()`：请求解析成功后调用 `requestHandler_`，生成待发送响应元信息
    - `handleWrite()`：发送阶段使用 `pendingStatusCode_ / pendingReasonPhrase_` 回包
- 服务层透传处理器：
  - `TcpServer.hpp/.cpp`
    - `setRequestHandler(...)`：统一注入业务处理入口
    - `acceptConnection()`：新连接建立后把处理器下发到对应 `TcpConnection`
- 入口示例挂载：
  - `main.cpp`
    - 通过 `server.setRequestHandler(...)` 提供默认示例（`/healthz` 返回 200）
    - 该位置即后续第三方适配层挂载点

## 前后对比
- 上一版（Iteration 18）：
  - 有连接策略和 keepalive 护栏，但业务处理逻辑仍内嵌在连接读写链路，扩展入口弱。
- 本版（Iteration 19）：
  - Reactor 继续只负责 IO 事件与生命周期。
  - 业务响应决策抽象为 handler，可独立替换为 HTTP Router、SQL 访问层、Redis 客户端封装。

## 术语补充
- `RequestHandler`：请求处理函数，输入解析后的请求对象，输出响应元信息。
- `平替`：保持外部接口不变，替换内部具体实现（如从演示 handler 切到第三方库 adapter）。

## 验证结果
- `make -j4`：通过。
- `scripts/test_smoke.sh http://127.0.0.1:18094/`：`PASS`（200）。

## 相比上一版本解决的问题
- 在不改 Reactor 核心的情况下，补上了“业务处理可插拔”这一基础框架能力。
- 为后续第三方库接入提供了稳定挂点，避免把业务细节反向侵入事件循环。
