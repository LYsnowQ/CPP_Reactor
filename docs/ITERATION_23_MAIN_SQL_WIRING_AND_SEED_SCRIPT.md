# Iteration 23 - 主入口 SQL 接线与数据注入脚本

## 本轮目标
- 将 `SqlHandler` 接入 `main.cpp` 启动链路，实现 SQL 查询与静态文件服务的复合路由。
- 添加 `seed_data.py` 数据注入脚本，为测试提供初始数据集。

## 技术点与代码位置

### 1. 主入口 SQL 接线（main.cpp）
- [main.cpp](file:///project/CPPReactor/src/app/main.cpp#L1-L125)：新增以下 SQL 集成流程：
  - **SQL 配置加载**：通过 `JsonConfigLoader::loadJsonFileOrThrow()` 读取 `config/SQLConfig.json`
  - **SqlExecutor 创建**：以 `ThreadLocalSingleConn`（`MySqlConnection` 工厂）构造并启动
  - **复合请求处理器**：lambda 路由根据 URL 路径分发：
    - `/query` 或 `/sql` 前缀 → `SqlHandler`
    - 其他路径 → `StaticFileHandler`
  - **新增 includes**：`SqlHandler.hpp`、`ISqlConnection.hpp`、`SqlExecutor.hpp`、`MySqlConnection.hpp`、`ThreadLocalSqlConn.hpp`、`JsonConfigLoader.hpp`、`CoreStatus.hpp`
  - **移除旧注释块**：原单一静态处理器路径已注释/移除

### 2. 数据注入脚本
- [seed_data.py](file:///project/CPPReactor/scripts/seed_data.py#L1-L170)：Python 脚本用于向 MySQL 插入测试数据，特性如下：
  - 支持从 `config/SQLConfig.json` 读取配置，也可通过命令行参数覆盖
  - 创建 `users` 表（`id`, `name`, `email`, `created_at`）
  - 默认插入 50,000 行测试数据，支持批量提交（默认每批 1000 行）
  - 实时进度显示（行数/百分比/速率/耗时）

## 复合路由流程
```
HTTP 请求 → main.cpp lambda handler
  ├─ URL 以 /query 或 /sql 开头 → SqlHandler (异步 SQL 查询 → JSON 响应)
  └─ 其他路径 → StaticFileHandler (静态文件/目录索引)
```

## 相比上一版本解决的问题
- 相比 Iteration 22（SqlHandler 独立实现），本轮将 SQL 能力实际接入服务入口：
  1. **启动链路闭环**：`main.cpp` 不再仅挂载静态文件处理器，而是完整加载 SQL 配置 → 初始化连接池 → 启动执行器 → 注册复合路由
  2. **URL 路由雏形**：以 lambda 闭包实现了简单的路径前缀匹配分发，为后续 `HttpRouter` 独立组件提供了过渡方案
  3. **数据准备能力**：`seed_data.py` 为 SQL 查询处理器提供可复现的测试数据集，便于本地开发与压测验证
