# Iteration 09 - 方案A：连接级解析上下文（In-Flight Parser）

## 本轮目标
- 按方案 A 修复 `Content-Length` 分片场景下的解析错位问题。
- 让同一连接在多次读事件之间复用同一个请求解析上下文，而不是每次重建。
- 保持小步改造，不引入大规模框架重写。

## 改动理由（为什么要这样改）
- 改动前问题：
  - 请求头已被消费后，body 未收全返回 `kAgain`。
  - 下一次读事件若重新新建解析器，会把剩余 body 当成“新的请求行”解析，导致错位失败。
- 改动原理：
  - 将“解析状态机 + 中间状态”绑定到连接对象（`TcpConnection`）生命周期。
  - 当解析返回 `kAgain` 时保留上下文，下一次读事件继续从原状态推进。

## 技术点与代码位置

### 1) `HttpRequest::parseRequest` 支持外部 in-flight 上下文
- 技术点：
  - 新增可选参数 `std::unique_ptr<HttpRequest>* inFlight`。
  - 若传入上下文且已存在对象，则复用该对象继续解析；否则创建新对象。
  - `kOk` 时把对象移动给结果；`kAgain` 时继续留在 in-flight 中。
- 代码位置：
  - `HttpRequest.hpp`：`parseRequest(...)` 声明。
  - `HttpRequest.cpp`：`parseRequest(...)` 实现。

### 2) `parse_()` 改为按当前状态推进
- 技术点：
  - 不再每次固定执行“请求行 -> 请求头 -> body”全流程。
  - 根据 `curState_` 仅推进当前阶段，支持 `kParseReqBody` 阶段跨读事件续跑。
- 代码位置：
  - `HttpRequest.cpp`：`parse_()`。

### 3) `TcpConnection` 持有解析上下文
- 技术点：
  - 新增成员 `inFlightRequest_`。
  - `handleRead()` 调用 `parseRequest(&readBuffer_, &inFlightRequest_)`。
  - `kAgain` 保留上下文；`kInvalid` 清理上下文；`kOk` 转移到 `request_`。
- 代码位置：
  - `TcpConnection.hpp`：`inFlightRequest_` 成员。
  - `TcpConnection.cpp`：`handleRead()`。

## 本轮收益
- 修复了 `Content-Length` 分片时“头部已消费、下次从 body 误当请求行”的核心逻辑缺陷。
- 为后续接入第三方解析器提供了更稳定的连接级状态承载点（可替换为 parser 接口对象）。

## 相比上一版本解决的问题
- 相比 Iteration 08，本轮解决了“增量解析在跨读事件时上下文丢失”的关键正确性问题。
- 从“能解析完整包”提升到“能持续解析分片包”，实用性明显提升。

## 后续建议（下一轮）
- 抽象 `IHttpParser` 接口，将当前 `HttpRequest` 解析器封装为默认实现。
- 在 `TcpConnection` 注入 parser 工厂，为接入第三方解析器（llhttp/http-parser）做无侵入切换。
