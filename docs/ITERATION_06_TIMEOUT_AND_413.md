# Iteration 06 - 解析等待超时与413语义

## 本轮目标
- 为超大请求体建立专门响应语义（`413 Payload Too Large`）。
- 为增量解析等待建立轻量超时保护（`408 Request Timeout`）。

## 技术点与代码位置

### 1) 解析结果增加“超大包”标记
- 技术点：
  - `HttpRequest::ParseResult` 新增 `tooLarge` 字段，用于区分“普通非法请求”和“超大包”。
  - `parseBody_()` 超出 `kMaxBodyBytes` 时设置内部标记，并在 `parseRequest()` 映射到 `tooLarge=true`。
- 代码位置：
  - `HttpRequest.hpp`：`ParseResult::tooLarge`、`bodyTooLarge_`。
  - `HttpRequest.cpp`：`parseBody_()` 与 `parseRequest()` 映射逻辑。

### 2) `TcpConnection` 错误响应分流（400/413）
- 技术点：
  - 当解析失败且 `tooLarge=true` 时返回 `413`。
  - 其他非法请求保持 `400`，兼容既有行为。
- 代码位置：
  - `TcpConnection.cpp`：`handleRead()` 中 `parseResult` 分流。

### 3) 增量解析等待超时（轻量版）
- 技术点：
  - 连接维护 `parseWaiting_ + parseWaitStart_`。
  - `kAgain` 首次出现时开启等待计时；
  - 连续等待超过 5 秒回 `408 Request Timeout`。
- 代码位置：
  - `TcpConnection.hpp`：新增等待状态成员与 `isParseWaitTimeout_()`。
  - `TcpConnection.cpp`：`handleRead()` 与 `isParseWaitTimeout_()`。

## 本轮收益
- 请求解析错误语义更清晰：`400`（非法格式）、`413`（超大包）、`408`（等待超时）。
- 在不引入复杂计时器框架前提下，补齐了基础连接保护能力。

## 已知边界（下一轮处理）
- 当前 `408` 触发依赖后续读事件（轻量方案），非独立定时器驱动。
- 响应模板仍在连接层硬编码，后续可统一下沉到响应构建模块。

## 相比上一版本解决的问题
- 相比 Iteration 05，本轮把协议防护从“构建可用”推进到“运行期可保护”，补上 413 与 408 的最小语义闭环。
- 区分了非法请求与超大包请求，避免所有错误都被压成同一类 400。
