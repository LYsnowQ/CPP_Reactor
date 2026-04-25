# Iteration 04 - Body安全收口（长度上限）

## 本轮目标
- 为 `Content-Length` 路径增加基础防护，避免超大 body 直接压垮连接缓冲策略。
- 修复请求体读取接口可用性，确保上层能拿到真实 body 数据。

## 技术点与代码位置

### 1) 增加 body 长度上限
- 技术点：
  - 在 `HttpRequest` 中定义 `kMaxBodyBytes`（当前为 1MB）。
  - 当 `Content-Length` 超过上限时，直接返回 `kInvalid`。
- 代码位置：
  - `HttpRequest.hpp`：`kMaxBodyBytes` 常量。
  - `HttpRequest.cpp`：`parseBody_()` 中上限校验。

### 2) `Content-Length` 去空白后再解析
- 技术点：
  - 解析前先裁剪 header 值前后空白，避免 `" 123 "` 这种值被误判。
  - 保持 `from_chars` 的严格合法性校验。
- 代码位置：
  - `HttpRequest.cpp`：`parseBody_()` 中 `trim + from_chars` 逻辑。

### 3) 修复 body 读取接口
- 技术点：
  - `getBody()` 由固定空串改为返回 `body_` 实际内容。
- 代码位置：
  - `HttpRequest.cpp`：`getBody()`。

## 本轮收益
- `Content-Length` 路径具备基础“拒绝超大包”能力，减少异常请求对系统冲击。
- 为后续业务层（CRUD/JSON）读取请求体提供直接可用接口。

## 已知边界（下一轮处理）
- 暂未加入连接级“body 等待超时”，仅通过 `kAgain` 等待后续数据。
- 暂未细分“超大包”专门错误码（当前归入 `kInvalid`）。

## 相比上一版本解决的问题
- 相比 Iteration 03，本轮解决了“可收 body 但缺少上限保护”的风险，增加超大包拦截能力。
- 修复了 `getBody()` 可用性问题，使上层业务能读取真实 body 内容而非空串。
