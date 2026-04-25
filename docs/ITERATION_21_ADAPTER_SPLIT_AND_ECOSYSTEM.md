# Iteration 21 - 适配器拆分与生态选型

## 本轮目标
- 将静态目录演示从 `main.cpp` 抽离为独立适配器，保持 Reactor 核心与演示逻辑解耦。
- 给出下一阶段第三方库接入方向（HTTP/SQL/Redis）。

## 技术点与代码位置
- 新增静态适配器：
  - `StaticFileAdapter.hpp`：定义 `StaticFileAdapter::createHandler(...)`
  - `StaticFileAdapter.cpp`：实现目录索引、文件读取、MIME 判定、`/healthz`、路径越界防护
- 精简入口组装：
  - `main.cpp`：仅保留参数解析、`TcpServer` 启动、`setRequestHandler(...)` 挂载
  - 从 `main.cpp` 移除大量 URL/HTML/文件处理细节逻辑

## 验证结果
- `make -j4`：通过
- `scripts/test_smoke.sh http://127.0.0.1:18096/`：通过
- `scripts/test_static_browser.sh http://127.0.0.1:18096`：通过

## 生态选型（摘要）
- HTTP 轻量接入：`cpp-httplib`（单头文件，接入快）
- HTTP 高性能框架：`Drogon`（异步框架，功能全）
- HTTP 底层可控：`Boost.Beast`（协议层能力强，学习门槛较高）
- PostgreSQL：`libpqxx`
- Redis：`redis-plus-plus`（基于 hiredis）
- 多数据库统一抽象：`SOCI`

## 相比上一版本解决的问题
- 相比 Iteration 20，本轮把“演示逻辑”从入口文件剥离，后续替换 SQL/Redis/第三方 HTTP handler 时不需要继续堆积在 `main.cpp`。
