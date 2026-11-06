# Iteration 03 - HTTP Body最小可用（Content-Length）

## 本轮目标
- 在现有增量解析链路上补齐 `parseBody_()` 的最小可用能力。
- 将 `Content-Length` 接入状态机，继续沿用 `kAgain/kInvalid/kOk` 语义。
- 保持小步改造，不引入 chunked 或复杂传输编码。

## 技术点与代码位置

### 1) `parseBody_()` 接入三态状态码
- 技术点：
  - 无 `Content-Length`：视为无 body，直接 `kOk`。
  - `Content-Length` 非法：返回 `kInvalid`。
  - body 未收全：返回 `kAgain`，等待后续数据。
  - body 收全：消费对应字节并返回 `kOk`。
- 代码位置：
  - `HttpRequest.cpp`：`parseBody_()`。

### 2) Header 查找能力（大小写不敏感）
- 技术点：
  - 新增 `findHeader_()`，对 header key 做大小写不敏感匹配。
  - 用于解析 `Content-Length`，避免上层重复扫描 headers。
- 代码位置：
  - `HttpRequest.hpp`：`findHeader_()` 声明。
  - `HttpRequest.cpp`：`findHeader_()` 实现。

### 3) 使用 `from_chars` 解析长度字段
- 技术点：
  - 使用 `std::from_chars` 进行无分配数值解析。
  - 通过 `errc` 与终止位置校验保证字段完全合法。
- 代码位置：
  - `HttpRequest.cpp`：`parseBody_()` 中长度解析逻辑。

## 本轮收益
- HTTP 请求从“仅请求行/请求头增量解析”提升到“支持固定长度 body 的增量解析”。
- 为下一步接入业务层 CRUD 请求体（JSON/Form）打下基础。

## 已知边界（下一轮处理）
- 暂不支持 `Transfer-Encoding: chunked`。
- 暂未限制 body 最大长度，后续建议增加上限防护。

## 相比上一版本解决的问题
- 相比 Iteration 02，本轮补齐了 body 路径，解决了“只能解析行和头、无法处理请求体”的缺口。
- 解析器从“头部增量可用”推进到“定长 body 增量可用”，可承接基础 CRUD 请求体场景。
