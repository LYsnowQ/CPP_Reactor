# 基准压测报告

## 环境信息

### 硬件

| 项目 | 规格 |
|------|------|
| CPU | 13th Gen Intel(R) Core(TM) i7-13700H |
| 核心数 | 20（物理核+超线程） |
| 内存 | 15 GiB |
| 平台 | WSL2 (Windows Subsystem for Linux) |

### 软件

| 项目 | 版本 |
|------|------|
| 操作系统 | Ubuntu 22.04.5 LTS (Jammy) |
| 内核 | Linux 5.15.167.4-microsoft-standard-WSL2 |
| 编译器 | g++ 11.4.0 |
| C++ 标准 | C++20 |
| 构建工具 | Make |
| MySQL Connector/C++ | 9.7.0 (libmysqlcppconn10) |
| 压测工具 | wrk 4.1.0 (epoll) |
| 编译优化 | -g (Debug) |
| 链接库 | -lmysqlcppconn -pthread |

### 远程数据库

| 项目 | 规格 |
|------|------|
| 类型 | 腾讯云 MySQL 单节点(云盘) |
| 配置 | 基础型 1核 1000MB / 20GB |
| 版本 | MySQL 5.7.18-txsql-log |
| 引擎 | InnoDB |
| 连接方式 | 公网 TCP |
| 测试数据 | users 表 50,000 行 |

---

## 1. 静态文件压测（无 SQL）

### 运行命令

```bash
bash tests/bench_static.sh . M
```

### 结果

| case | dispatcher | io_threads | conn_mode | wrk_threads | connections | duration | 总请求 | QPS | 平均延迟 | 传输速率 | 套接字错误 |
|------|-----------|----------:|----------|-----------:|----------:|:--------|------:|----:|--------:|--------:|----------|
| M1 | epoll | 4 | keepalive | 4 | 150 | 12s | 111,092 | 7,669 | 15.83ms | 8.40MB/s | connect 0, read 1042, write 0, timeout 148 |
| M2 | epoll | 6 | keepalive | 6 | 240 | 12s | 95,158 | 7,906 | 30.45ms | 8.66MB/s | connect 0, read 846, write 0, timeout 0 |
| M3 | epoll | 6 | close | 6 | 240 | 12s | 122,495 | 10,143 | 23.64ms | 11.07MB/s | connect 0, read 5978, write 0, timeout 0 |
| M4 | poll | 6 | keepalive | 6 | 240 | 12s | 134,141 | 9,252 | 21.59ms | 10.14MB/s | connect 0, read 1202, write 0, timeout 240 |

### 分析

- **最高 QPS: 10,143**（M3: epoll 6线程 close 模式）
- **最低延迟: 15.83ms**（M1: epoll 4线程 keepalive）
- epoll 与 poll 性能接近
- close 模式比 keepalive 高约 15%（因请求更简单，但 socket 错误也更多）

---

## 2. SQL 远程查询压测

### 运行命令

```bash
bash tests/bench_sql.sh . S
```

### 结果

| 用例 | wrk线程 | 并发数 | duration | 总请求 | QPS | 平均延迟 | 延迟分布 |
|------|-------:|------:|:--------|------:|----:|--------:|---------|
| `SELECT count(*) FROM users` | 1 | 5 | 15s | 229 | 15.25 | 324ms | p50=321ms / p99=414ms |
| `SELECT * FROM users WHERE name=...` | 1 | 5 | 15s | 204 | 13.58 | 364ms | p50=357ms / p99=464ms |
| `SELECT * FROM users LIMIT 100` | 2 | 10 | 15s | 262 | 17.45 | 562ms | p50=558ms / p99=784ms |

### 分析

- **QPS 瓶颈在远程数据库**，不在服务器本身（静态文件压测 QPS ~10,000 vs SQL ~15）
- 每次 SQL 查询需 ~300-800ms，其中绝大部分为**网络往返时间**
- 单核远程 MySQL 5.7 的处理能力限制了总吞吐
- SqlExecutor 单线程串行执行，查询在 worker 线程中排队

---

## 3. 第三方连接测试（独立程序）

### 测试内容

独立测试程序 `conn_compare`（存于 `/tmp/mysql_test/`）验证了两种 JDBC 连接方式：

| 方式 | 主线程 | 后台线程 | 并发4线程 | 连续查询 | 结论 |
|------|-------|---------|---------|---------|------|
| `tcp://` URL 字符串 | ✅ PASS | ✅ PASS | ✅ PASS | ✅ PASS | 简洁但无法设置 OPT_RECONNECT |
| `ConnectOptionsMap` | ✅ PASS | ✅ PASS | ✅ PASS | ✅ PASS | 支持全部配置参数 |

**结论：** 两种方式均兼容 MySQL 5.7，推荐使用 Map 方式以最大化配置兼容性。

---

## 历史记录

| 日期 | 报告 | 备注 |
|------|------|------|
| 2026-06-02 | 本报告 | 首次完整记录 SQL + 静态压测 |
| 2026-04-25 | [bench_profile_M_20260425.md](benchmarks/bench_profile_M_20260425.md) | 初始静态压测基线 |
| 2026-04-25 | [bench_gradient_20260425_local.md](benchmarks/bench_gradient_20260425_local.md) | 梯度压测 |
| 2026-04-25 | [bench_matrix_20260425_local.md](benchmarks/bench_matrix_20260425_local.md) | 矩阵压测 |
