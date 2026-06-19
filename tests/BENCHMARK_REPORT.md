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
| 编译优化 | -O2 -g (Optimized) |
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
# 多配置探索
bash tests/bench.sh static M

# 梯度加压（逐步增加并发，到上限自动停止）
bash tests/bench.sh static GRADIENT
```

### 结果（优化后，编译 -O2 + 线程调优）

**M 探索（epoll 12th keepalive 基准）：**

| case | dispatcher | io_threads | conn_mode | wrk_threads | connections | duration | 总请求 | QPS | 平均延迟 | 传输速率 | 套接字错误 |
|------|-----------|----------:|----------|-----------:|----------:|:--------|------:|----:|--------:|--------:|----------|
| M1 | epoll | 8 | keepalive | 8 | 300 | 8s | 390,660 | 35,593 | 6.32ms | 46.98MB/s | connect 0, read 3766, write 0, timeout 294 |
| M2 | epoll | 12 | keepalive | 12 | 500 | 8s | 300,659 | 37,116 | 12.95ms | 48.99MB/s | connect 0, read 2794, write 0, timeout 0 |
| M3 | epoll | 16 | keepalive | 16 | 800 | 8s | 356,292 | 44,120 | 18.16ms | 58.23MB/s | connect 0, read 3210, write 0, timeout 0 |
| M4 | epoll | 12 | close | 12 | 500 | 8s | 197,699 | 24,461 | 8.81ms | 32.17MB/s | connect 0, read 8768, write 0, timeout 0 |
| M5 | poll | 12 | keepalive | 12 | 500 | 8s | 417,415 | 51,537 | 9.63ms | 68.02MB/s | connect 0, read 3947, write 0, timeout 0 |

**GRADIENT 梯度（epoll 12 threads keepalive）：**

| 并发数 | wrk 线程 | QPS | 平均延迟 |
|-------|---------|-----|---------|
| 50 | 4 | 41,421 | 0.79ms |
| 100 | 4 | 60,061 | 1.65ms |
| 200 | 8 | **60,645** | 3.32ms |
| 500 | 12 | 58,423 | 8.94ms |

### 分析

- **最高 QPS: 60,645**（GRADIENT: 200 并发，epoll 12 keepalive）
- **相比优化前（10,143 QPS）提升约 6 倍**，主要得益于 `-O2` 编译优化和线程数调整
- close 模式（M4: 24,461 QPS）仍受单线程 accept 限制

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

| 日期 | 备注 |
|------|------|
| 2026-06-19 | 编译优化 -O2 + 线程调优，QPS 10k→60k；Echo 基准 1.78M QPS；脚本合并为 bench.sh |
| 2026-06-02 | 首次完整记录 SQL + 静态压测（编译 -O0，QPS ~10k） |
| 2026-04-25 | 初始静态压测基线 |
