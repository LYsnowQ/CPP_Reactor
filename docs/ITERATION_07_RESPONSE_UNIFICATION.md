# Iteration 07 - 连接层响应构建统一化

## 本轮目标
- 收敛 `TcpConnection` 内散落的状态响应拼接逻辑。
- 让连接层更聚焦“状态分发”，减少重复字符串与重复队列代码。

## 技术点与代码位置

### 1) 抽取统一响应构建函数
- 技术点：
  - 新增 `appendSimpleResponse_(statusCode, reasonPhrase)`，统一构造最小 HTTP 响应行。
  - 用于 `200/400/408/413` 的基础响应模板，避免多处硬编码。
- 代码位置：
  - `TcpConnection.hpp`：方法声明。
  - `TcpConnection.cpp`：方法实现。

### 2) 抽取统一“回包并切写事件”函数
- 技术点：
  - 新增 `queueSimpleResponse_(...)`，内部统一执行“写入缓冲 + MODIFY”。
  - 解析异常路径不再重复 `append + addTask(MODIFY)`。
- 代码位置：
  - `TcpConnection.hpp`：方法声明。
  - `TcpConnection.cpp`：方法实现与调用替换。

### 3) 调用点统一替换
- 技术点：
  - `408` 超时路径改为 `queueSimpleResponse_(408, ...)`。
  - `413/400` 错误路径改为 `queueSimpleResponse_(...)`。
  - `200` 正常写路径改为 `appendSimpleResponse_(200, ...)`。
- 代码位置：
  - `TcpConnection.cpp`：`handleRead()`、`handleWrite()`。

## 本轮收益
- 连接层响应逻辑由“分散 if-else 拼串”收敛到两个小函数，后续扩展更稳定。
- 为后续“下沉到专门响应构建模块”预留了过渡形态。

## 已知边界（下一轮处理）
- 目前仍是“最小 HTTP 响应”模板，尚未统一附加公共头（如 `Connection/Content-Length`）。
- 连接层仍直接决定响应码，业务层尚未接管错误映射策略。

## 相比上一版本解决的问题
- 相比 Iteration 06，本轮解决了连接层响应逻辑分散、重复拼接的问题，降低后续维护成本。
- 将“状态判定”和“响应构建”初步分离，为后续模块化下沉响应器打下接口基础。
