# Iteration 02 - HTTP增量解析状态收敛（增量记录）

## 本轮目标
- 将 HTTP 请求解析从“二值成功/失败”升级为“可重试/非法/成功”三态。
- 避免 TCP 读链路把“半包”误判成 `400 Bad Request`。
- 保持现有架构不推翻，仅做协议层与接入层的最小闭环改造。

## 技术点与代码位置

### 1) 解析结果结构化（状态码 + 请求对象 + 信息）
- 技术点：
  - `HttpRequest::parseRequest` 返回 `ParseResult`，包含 `StatusCode`、解析对象、说明信息。
  - 使用 `core::StatusCode` 统一语义：`kOk`、`kAgain`、`kInvalid`。
- 代码位置：
  - `HttpRequest.hpp`：`struct ParseResult`、`parseRequest` 声明、`parse_/parseLine_/parseHead_/parseBody_` 返回值改为 `StatusCode`。
  - `HttpRequest.cpp`：`parseRequest` 实现与状态映射。

### 2) 请求行解析支持“未收全重试”
- 技术点：
  - 未找到 `\r\n` 返回 `kAgain`，表示继续收包，不再直接判错。
  - 仅在语法结构错误时返回 `kInvalid`。
  - 请求行先视图解析，验证成功后再消费 Buffer，避免提前消费导致状态错乱。
- 代码位置：
  - `HttpRequest.cpp`：`parseLine_()`。

### 3) 请求头解析支持“未收全重试”
- 技术点：
  - 未找到头结束符 `\r\n\r\n` 返回 `kAgain`。
  - 单行头部格式错误（缺少 `: `）返回 `kInvalid`，避免静默吞错。
  - 逐行使用视图读取并在处理后消费，保持增量解析一致性。
- 代码位置：
  - `HttpRequest.cpp`：`parseHead_()`。

### 4) TCP读链路对状态码分流
- 技术点：
  - `kAgain`：直接返回等待后续数据，不触发写回。
  - 非 `kOk`（如 `kInvalid`）：写入 `400 Bad Request` 并切换写事件。
  - `kOk`：保存请求对象并进入后续写流程（当前仍是临时响应逻辑）。
- 代码位置：
  - `TcpConnection.cpp`：`handleRead()` 中 `parseResult` 分流逻辑。

### 5) 与你当前简化版TcpServer接口对齐
- 技术点：
  - 入口调用改为 `server.run()`，避免旧枚举参数残留导致接口不一致。
- 代码位置：
  - `main.cpp`：`run` 调用处。

## 本轮收益
- 半包不再触发误判 400，HTTP 解析链路具备基础增量能力。
- 状态码语义从 EventLoop/Dispatcher 向协议层延伸，便于后续统一错误模型。

## 已知边界（下一轮处理）
- `parseBody_()` 仍是占位，尚未接入 `Content-Length`。
- 业务响应仍是临时固定策略，尚未接入路由/handler 抽象。

## 相比上一版本解决的问题
- 相比 Iteration 01，本轮解决了“半包直接 400”的误判问题，把协议解析从二值失败升级为三态语义。
- 读链路首次具备“继续等待而非立即失败”的能力，为后续 body 增量处理打通前提。
