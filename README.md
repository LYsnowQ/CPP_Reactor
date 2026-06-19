<div align="center">

# CPPReactor

高性能 C++20 Reactor 模式 HTTP 服务器框架

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](#许可证)
[![Dispatcher](https://img.shields.io/badge/reactor-epoll%20%7C%20poll%20%7C%20select-brightgreen)](#核心特性)

</div>

## 目录

- [项目简介](#项目简介)
- [核心特性](#核心特性)
- [架构概览](#架构概览)
- [环境要求](#环境要求)
- [快速开始](#快速开始)
  - [使用 Make（推荐）](#使用-make推荐)
  - [使用 CMake + Conan](#使用-cmake--conan)
- [配置说明](#配置说明)
  - [SQL 配置](#sql-配置)
  - [命令行参数](#命令行参数)
- [Docker 部署](#docker-部署)
- [测试与压测](#测试与压测)
  - [功能测试](#功能测试)
  - [性能压测](#性能压测)
  - [压测数据注入](#压测数据注入)
- [项目结构](#项目结构)
- [压测报告摘要](#压测报告摘要)
- [技术栈](#技术栈)
- [开发指南](#开发指南)
- [许可证](#许可证)

---

## 项目简介

CPPReactor 是一款从零实现的 C++20 高性能 HTTP 服务器框架，核心采用 **Reactor 多线程架构**，支持 **epoll/poll/select** 三种事件分发器动态切换。项目同时集成了 **MySQL 异步查询** 能力，适用于构建轻量级 RESTful API 网关或静态资源服务器。

> 该项目是 C++ 网络编程学习实践的产物，经历了数十次迭代演进，涵盖了完整的 TCP 连接生命周期管理、HTTP 协议解析、Reactor 多线程模型、连接池、可观测性等主题。

### 性能概览

| 场景 | 峰值 QPS | 说明 |
|------|---------|------|
| **Echo 基准**（无业务逻辑） | **1,781,742** | 框架纯 IO 吞吐上限，验证 epoll Reactor 架构可达百万级 |
| **HTTP 静态文件**（目录索引） | **60,645** | 含 HTTP 解析 + 文件遍历 + 响应拼装，受业务代码开销限制 |
| **SQL 远程查询** | 链路验证 | 公网连接测试，非性能基准 |

> Echo 与 HTTP 之间约 **29 倍**的差距，全部来源于**业务代码开销**（HTTP 解析、字符串分配、目录 IO、响应拼接），而非架构选型问题。这意味着将业务逻辑剥离为独立中间件层，让框架专注 IO 调度，可获得数量级的性能提升。

---

## 核心特性

| 特性 | 说明 |
|------|------|
| **Reactor 多线程模型** | 主线程 accept，多 IO 线程事件循环，跨线程回调 |
| **多事件分发后端** | epoll / poll / select 运行时切换，通过命令行参数指定 |
| **HTTP/1.1 协议解析** | 支持请求行、请求头、Content-Length Body 完整解析 |
| **Keep-Alive 连接管理** | 支持连接复用、最大请求数限制、空闲超时 |
| **静态文件服务** | 目录索引 + 文件 Serve，支持大文件分片传输 |
| **MySQL SQL 查询** | 基于 MySQL Connector/C++，线程本地连接池，异步回调 |
| **统一状态码** | 全框架统一的 StatusCode 返回值语义 |
| **结构化日志** | 基于 spdlog，支持多级别、多 Sink |
| **JSON 配置** | 基于 nlohmann_json 的配置文件加载 |
| **容器化部署** | 提供 Dockerfile + docker-compose 一键部署 |
| **全链路测试** | 冒烟测试、HTTP 边界测试、多档位压测脚本 |
| **可观测性** | 内置 Metrics 采集、慢查询日志 |

---

## 架构概览

```
┌──────────────────────────────────────────────────────────────┐
│                      main.cpp                                │
│  ┌──────────┐  ┌────────────────┐  ┌──────────────────────┐ │
│  │HttpRouter│  │  TcpServer     │  │  SqlExecutor         │ │
│  │          │  │  ┌──────────┐  │  │  ┌────────────────┐  │ │
│  │  / ─────►│  │  │ main     │  │  │  │  ThreadLocal    │  │ │
│  │  /query─►│  │  │ Reactor  │  │  │  │  SingleConn     │  │ │
│  │  /sql───►│  │  │ (accept) │  │  │  └──────┬─────────┘  │ │
│  └──────────┘  │  └────┬─────┘  │  │         │             │ │
│                 │       │        │  │  ┌──────▼─────────┐  │ │
│                 │  ┌────▼─────┐  │  │  │ MySqlConnection│  │ │
│                 │  │ IOThread │  │  │  │ (Connector/C++)│  │ │
│                 │  │ Pool     │  │  │  └────────────────┘  │ │
│                 │  │ ┌─────┐  │  │  └──────────────────────┘ │
│                 │  │ │EPoll│  │  │                           │
│                 │  │ │Poll │  │  │  ┌──────────────────────┐ │
│                 │  │ │Select│  │  │  │ StaticFileHandler   │ │
│                 │  │ └─────┘  │  │  │ (目录索引 + 文件)    │ │
│                 │  └─────────┘  │  └──────────────────────┘ │
│                 └───────────────┘                            │
└──────────────────────────────────────────────────────────────┘
```

### 请求处理流程

```
Client ──► TcpServer::accept()
             │
             ▼
         IOThreadPool 中的某个 EventLoop 线程
             │
             ▼
         Channel(TcpConnection) ──► epoll_wait 就绪事件
             │
             ├── 读事件：HttpRequest::parseRequest()
             │         │
             │         ▼
             │     HttpRouter::dispatch()
             │         │
             │         ├── /query, /sql ──► SqlHandler
             │         │                      └── SqlExecutor::queryAsync()
             │         │                           └── MySqlConnection
             │         │
             │         └── / ──► StaticFileHandler
             │
             └── 写事件：HttpResponse 回写响应
```

---

## 环境要求

### 编译环境

| 工具 | 最低版本 | 安装命令 |
|------|---------|---------|
| GCC | 11+ (支持 C++20) | `apt install g++` |
| Make | 4.x | `apt install make` |
| CMake | 3.16+ (可选) | `apt install cmake` |
| Python | 3.10+ (仅测试用) | `apt install python3` |

### 运行时依赖

| 依赖 | 说明 | 安装命令 |
|------|------|---------|
| MySQL Connector/C++ | MySQL 客户端库（已内嵌 `third_party/mysql-cppconn`） | 无需额外安装 |
| nlohmann_json | JSON 解析（已内嵌 `third_party/nlohmann_json`） | 无需额外安装 |
| spdlog (header-only) | 日志库（已内嵌 `third_party/spdlog`） | 无需额外安装 |
| pymysql | Python 测试数据注入 (仅测试用) | `pip install pymysql` |
| wrk | HTTP 压测工具 (仅压测用) | `apt install wrk` |

> **所有 C++ 依赖（MySQL Connector/C++、nlohmann_json、spdlog）均已内嵌在 `third_party/` 目录**，clone 后即可编译，无需额外安装系统包或使用 Conan。

---

## 快速开始

### 使用 Make（推荐）

```bash
# 1. 编译项目（所有依赖已内嵌在 third_party/）
make clean && make -j$(nproc)

# 2. 启动服务器（提供默认参数）
./build/main_run 8080 . epoll 4 close

# 3. 验证服务
curl http://127.0.0.1:8080/

# 4. 冒烟测试
bash tests/test_smoke.sh http://127.0.0.1:8080/
```

启动后服务器默认监听 `8080` 端口，提供 HTTP 服务。访问 `http://127.0.0.1:8080/` 可看到静态文件目录索引。

---

## 配置说明

### SQL 配置

数据库连接通过 JSON 文件配置，默认路径为 `config/SQLConfig.json`：

```bash
# 编辑配置文件
vim config/SQLConfig.json
```

配置项说明：

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `host` | string | `""` | MySQL 主机地址 |
| `port` | int | `3306` | MySQL 端口 |
| `user` | string | `"root"` | 数据库用户名 |
| `password` | string | `""` | 数据库密码 |
| `database` | string | `""` | 数据库名称 |
| `charset` | string | `"utf8mb4"` | 连接字符集 |
| `connectTimeoutMs` | int | `5000` | 连接超时（毫秒） |
| `readTimeoutMs` | int | `5000` | 读超时 |
| `writeTimeoutMs` | int | `5000` | 写超时 |
| `connMode` | string | `"thread_local_single_conn"` | 连接模式 |
| `pingBeforeUse` | bool | `true` | 使用前是否 Ping 检测 |
| `reconnectMaxAttempts` | int | `3` | 最大重连次数 |
| `reconnectBackoffMs` | int | `1000` | 重连间隔（毫秒） |
| `maxConnLifetimeMs` | int | `1800000` | 最大连接存活时间 |
| `maxConnIdleMs` | int | `60000` | 最大空闲时间 |
| `slowQueryMs` | int | `1000` | 慢查询阈值（毫秒） |
| `enableSqlLog` | bool | `true` | 是否启用 SQL 日志 |
| `readOnly` | bool | `true` | 是否只读模式 |

### 命令行参数

```bash
./build/main_run <port> <resource_path> [dispatcher] [threads] [conn_mode] [keepalive_max_reqs] [keepalive_idle_ms]
```

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `port` | 监听端口（必填） | — |
| `resource_path` | 静态资源根目录（必填） | — |
| `dispatcher` | 事件分发器：`epoll` / `poll` / `select` | `epoll` |
| `threads` | IO 工作线程数 | `4` |
| `conn_mode` | 连接模式：`close` / `keepalive` | `close` |
| `keepalive_max_reqs` | Keep-Alive 最大请求数 | `100` |
| `keepalive_idle_ms` | Keep-Alive 空闲超时（毫秒） | `10000` |

**示例：**

```bash
# 使用 epoll + 4 线程 + keepalive
./build/main_run 8080 /var/www epoll 4 keepalive 100 10000

# 使用 poll + 6 线程 + short-lived 连接
./build/main_run 8080 . poll 6 close
```

---

## Docker 部署

### 使用 docker-compose（推荐，含 MySQL）

```bash
# 启动 MySQL + 应用
docker-compose up -d

# 查看日志
docker-compose logs -f app

# 测试服务
curl http://localhost:8080/

# 停止
docker-compose down
```

### 单独构建和运行

```bash
# 构建镜像
docker build -t cppreactor .

# 运行（需自行准备 MySQL）
docker run -d \
  --name cppreactor \
  -p 8080:8080 \
  -e MYSQL_HOST=your-mysql-host \
  -e MYSQL_PORT=3306 \
  -e MYSQL_USER=root \
  -e MYSQL_PASSWORD=root \
  -e MYSQL_DATABASE=test \
  cppreactor
```

### 容器环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `MYSQL_HOST` | `mysql` | MySQL 主机 |
| `MYSQL_PORT` | `3306` | MySQL 端口 |
| `MYSQL_USER` | `root` | 数据库用户 |
| `MYSQL_PASSWORD` | `root` | 数据库密码 |
| `MYSQL_DATABASE` | `test` | 数据库名 |

容器启动时，[docker-entrypoint.sh](docker-entrypoint.sh) 会自动根据环境变量生成 `config/SQLConfig.json`。

---

## 测试与压测

详细的测试说明见 [tests/README.md](tests/README.md)。

### 功能测试

```bash
# 1. 编译并启动服务
make clean && make -j$(nproc)
./build/main_run 8080 . epoll 4 close &

# 2. 冒烟测试
bash tests/test_smoke.sh http://127.0.0.1:8080/

# 3. HTTP 协议边界测试（413 大包、半包、非法请求等）
bash tests/setup.sh                          # 首次：创建 Python 虚拟环境
tests/.venv/bin/python3 tests/run_http_cases.py 127.0.0.1 8080
```

### 压测数据注入

```bash
# 向 MySQL users 表注入 5 万行测试数据
bash tests/setup.sh
tests/.venv/bin/python3 tests/seed_data.py \
  --host 127.0.0.1 --port 3306 \
  --user root --password yourpass --database test
```

### 静态文件压测

```bash
# 多配置探索（M1-M5）
bash tests/bench.sh static M

# 梯度加压（逐步增加并发找到上限）
bash tests/bench.sh static GRADIENT

# 自定义每档时长（默认 12 秒）
bash tests/bench.sh static GRADIENT 8

# 输出: tests/benchmarks/bench_YYYYMMDD_HHMMSS.md
```

### Echo 基准压测

```bash
# 梯度加压（固定 12 线程，逐步增加并发）
bash tests/bench.sh echo ECHO

# 线程梯度（固定 500 并发，逐步增加服务器线程）
bash tests/bench.sh echo ECHO_THREADS
```

### SQL 查询压测

```bash
# S: 轻量 | M: 中等 | L: 重度 | ALL: 全量
bash tests/bench_sql.sh . S
# 输出: tests/benchmarks/bench_sql_S_latest.md
```

### 压测档位说明

#### bench.sh static M PROFILE

| 用例 | dispatcher | io_threads | 连接模式 | wrk 参数 | 说明 |
|------|-----------|-----------|---------|---------|------|
| M1 | epoll | 8 | keepalive | 8 线程 / 300 连接 | |
| M2 | epoll | 12 | keepalive | 12 线程 / 500 连接 | |
| M3 | epoll | 16 | keepalive | 16 线程 / 800 连接 | |
| M4 | epoll | 12 | close | 12 线程 / 500 连接 | 短连接模式 |
| M5 | poll | 12 | keepalive | 12 线程 / 500 连接 | 对比 poll 后端 |

#### bench_sql.sh PROFILE

| 档位 | 并发范围 | 说明 |
|------|---------|------|
| S | 1-2 wrk, 5-10 conn | 轻量：验证功能 + 基础 QPS |
| M | 2-4 wrk, 10-20 conn | 中等混合 |
| L | 4-6 wrk, 20-40 conn | 重度高并发 |
| ALL | 全量 | 覆盖所有用例 |

---

## 项目结构

```
CPPReactor/
├── src/                          # 源码目录
│   ├── app/
│   │   └── main.cpp              # 入口
│   ├── core/
│   │   ├── EventLoop.cpp          # 事件循环
│   │   ├── Buffer.cpp             # 缓冲区
│   │   ├── Dispatcher.cpp         # 分发器工厂
│   │   ├── EpollDispatcher.cpp    # epoll 封装
│   │   ├── PollDispatcher.cpp     # poll 封装
│   │   └── SelectDispatcher.cpp   # select 封装
│   ├── net/
│   │   ├── TcpServer.cpp          # TCP 服务器
│   │   ├── TcpConnection.cpp      # TCP 连接
│   │   ├── IOThreadPool.cpp       # IO 线程池
│   │   └── Channel.cpp            # IO 通道
│   ├── protocol/
│   │   ├── HttpRequest.cpp        # HTTP 请求解析
│   │   ├── HttpResponse.cpp       # HTTP 响应构建
│   │   └── HttpRouter.cpp         # HTTP 路由分发
│   ├── handler/
│   │   ├── StaticFileHandler.cpp  # 静态文件处理
│   │   └── SqlHandler.cpp         # SQL 查询处理
│   ├── persistence/
│   │   ├── MySqlConnection.cpp    # MySQL 连接
│   │   ├── SqlExecutor.cpp        # SQL 执行器
│   │   ├── SqlConfig.cpp          # SQL 配置加载
│   │   ├── ThreadLocalSqlConn.cpp # 线程本地连接池
│   │   └── SqlTransaction.cpp     # 事务管理
│   ├── observability/
│   │   └── Metrics.cpp            # 可观测性
│   └── utils/
│       └── JsonConfigLoader.cpp   # JSON 配置加载
│
├── include/                      # 头文件
│   ├── core/                     #   Buffer, EventLoop, Dispatcher, etc.
│   ├── net/                      #   TcpServer, TcpConnection, IOThreadPool, Channel
│   ├── protocol/                 #   HttpRequest, HttpResponse, HttpRouter
│   ├── handler/                  #   IHttpHandler, SqlHandler, StaticFileHandler
│   ├── persistence/              #   ISqlConnection, MySqlConnection, SqlExecutor, etc.
│   ├── observability/            #   Metrics
│   ├── repository/sql/           #   SqlTask, SqlRepository
│   └── utils/                    #   JsonConfigLoader, StringUtils
│
├── third_party/
│   ├── spdlog/                   # 内嵌 spdlog (header-only)
│   ├── nlohmann_json/            # 内嵌 nlohmann_json (header-only)
│   └── mysql-cppconn/            # 内嵌 MySQL Connector/C++
│       ├── include/              #   JDBC 头文件
│       └── lib/                  #   .so 运行时库
│
├── config/
│   └── SQLConfig.json            # SQL 连接配置模板
│
├── tests/                        # 测试目录
│   ├── README.md                 # 测试使用说明
│   ├── BENCHMARK_REPORT.md       # 基准压测报告
│   ├── setup.sh                  # Python 虚拟环境初始化
│   ├── requirements.txt          # Python 依赖
│   ├── seed_data.py              # 数据注入脚本
│   ├── test_smoke.sh             # 冒烟测试
│   ├── run_http_cases.py         # HTTP 边界测试
│   ├── bench.sh                  # 统一压测脚本（静态文件 + Echo 基准）
│   ├── bench_sql.sh              # SQL 压测
│   ├── benchmarks/               # 压测输出（gitignore）
│   └── bench_reports/            # 历史报告（gitignore）
│
├── docs/                         # 迭代日志
│   ├── ITERATION_01_LIFECYCLE.md
│   ├── ITERATION_02_PROTOCOL_STATUS.md
│   ├── ...                       # 共 27+ 个迭代文档
│   └── ITERATION_27_CONTRACT_WALL_IMPL_AND_DOCKER.md
│
├── Dockerfile                    # 多阶段构建镜像
├── docker-compose.yml            # MySQL + App 编排
├── docker-entrypoint.sh          # 容器入口（环境变量 → 配置文件）
├── .dockerignore
├── CMakeLists.txt                # CMake 构建
├── Makefile                      # Make 构建
├── conanfile.py                  # Conan 依赖管理
├── .clang-format                 # 代码格式
├── .clangd                       # clangd 配置
└── .gitignore
```

---

## 压测报告摘要

以下为项目在 **WSL2 (i7-13700H, 20 核, 15GiB 内存)** 环境下的压测数据摘要，编译优化 `-O2`。完整报告见 [tests/BENCHMARK_REPORT.md](tests/BENCHMARK_REPORT.md)。

### Echo 基准压测（框架 IO 吞吐上限）

服务器不做 HTTP 解析，仅 read → discard → write(固定 HTTP 200 空响应)，旨在测量框架的原始 IO 调度能力。使用 wrk 压测。

| 并发数 | QPS | P50 延迟 |
|-------|-----|---------|
| 10 | 180,964 | 39μs |
| 50 | 520,259 | 69μs |
| 100 | 633,195 | 100μs |
| 200 | **1,336,849** | 151μs |
| 800 | **1,781,742** | 803μs |
| 1000 | 1,626,674 | 1.50ms |

> epoll Reactor 架构在纯 IO 场景下实测可达 **178 万 QPS**，证明框架的 IO 调度能力本身并非瓶颈。

### 静态文件压测（目录索引）

服务器执行完整 HTTP 解析 + 目录遍历 + 文件排序 + HTML 拼接 + 响应组装，体现真实业务场景。

| 模式 | QPS | 平均延迟 |
|------|-----|---------|
| epoll 8 线程 keepalive | 35,593 | 6.32ms |
| epoll 12 线程 keepalive | 37,116 | 12.95ms |
| epoll 16 线程 keepalive | 44,120 | 18.16ms |
| epoll 12 线程 keepalive（梯度 200 并发） | **60,645** | 3.32ms |
| poll 12 线程 keepalive | 51,537 | 9.63ms |

### SQL 远程查询（连接性验证）

通过公网连接远程腾讯云 MySQL 5.7（1 核 1GB）验证 SQL 查询链路可用性，**非性能测试**。

| 查询类型 | 平均延迟 | 说明 |
|---------|---------|------|
| `SELECT count(*)` | 324ms | 链路往返延迟为主 |
| `SELECT * WHERE name=` | 364ms | |
| `SELECT LIMIT 100` | 562ms | |

> 延迟 300-800ms 中 **95% 以上为公网 TCP 往返时间**。此数据仅用于验证 SQL 查询功能链路完整可用，不代表框架或数据库的真实处理能力。

---

## 性能分析：框架能力 vs 业务瓶颈

从以上两组压测数据可以清晰看到：

```
框架 IO 吞吐（Echo）    1,781,742 QPS  ← 架构能力
         ↓ 约 29 倍差距
HTTP 业务处理（静态文件）   60,645 QPS  ← 业务代码开销
```

**核心结论**：

1. **epoll Reactor 不是瓶颈** — 纯 IO 场景 178 万 QPS 证明架构选型合理
2. **业务代码是主要开销来源** — HTTP 解析、字符串分配、文件 IO 占据了约 96% 的 CPU 时间
3. **SQL 查询已验证链路可用** — 框架集成 MySQL 查询功能完整可用，性能取决于内网数据库环境

### 中间件架构展望

目前所有业务逻辑（HTTP 解析、路由、静态文件处理、SQL 查询）全部在框架内部完成，这与高性能服务器的最佳实践——**专注 IO，业务外置**——相悖。后续演进方向：

```
当前架构                   改进方向
┌──────────────┐          ┌──────────────┐
│  CPPReactor  │          │  CPPReactor  │  ← 专注：TCP 管理、TLS、协议帧
│  + HTTP 解析 │          │  (IO 调度层)  │     epoll 调度、连接池
│  + 路由分发  │    →     ├──────────────┤
│  + 静态文件  │          │  业务中间件    │  ← 可插拔：HTTP 解析、路由、限流
│  + SQL 查询  │          ├──────────────┤
│  + 业务逻辑  │          │  后端服务      │  ← 独立：文件服务、SQL 引擎、API
└──────────────┘          └──────────────┘
```

将 HTTP 协议解析、请求路由、业务处理等剥离为独立的中间件层后，框架可专注于 IO 调度，同时获得：
- **业务模块热更新**：中间件独立进程，重启不影响框架
- **语言无关**：中间件可用任意语言实现，通过 RPC 或共享内存通信
- **独立扩缩容**：IO 层和后端可独立调整线程数
- **性能隔离**：业务代码的 OOM、死循环不拖垮 IO 层

---

## 技术栈

| 领域 | 技术 |
|------|------|
| 语言 | C++20 |
| 网络编程 | Linux Socket API, epoll/poll/select |
| 并发模型 | Reactor 多线程模型（主 Reactor + 多 IO 线程） |
| 数据库 | MySQL Connector/C++ 9.7 / JDBC |
| 日志 | spdlog 1.14 (header-only) |
| JSON | nlohmann_json 3.11 |
| 构建 | Make |
| 容器 | Docker, docker-compose |
| 测试 | curl, Python socket, wrk |
| 代码工具 | clang-format, clangd |

---

## 开发指南

### 代码风格

项目使用 `.clang-format` 配置代码格式化：

```bash
# 格式化所有源码
clang-format -i src/**/*.cpp include/**/*.hpp
```

格式规则摘要：
- 基于 LLVM 风格，缩进 4 空格
- Allman 大括号风格
- 行宽 100 字符
- 禁止 Tab

### 添加新路由

```cpp
// 在 main.cpp 中注册
router.get("/api/hello", myHandler);
router.post("/api/data", myPostHandler);
router.addPrefix("/static", staticHandler);
```

### 添加新 Dispatcher 后端

1. 在 `include/core/` 下创建新的 Dispatcher 头文件
2. 在 `src/core/` 下创建对应的实现文件
3. 在 `DispatcherType` 枚举中添加新类型
4. 在 `Dispatcher` 工厂方法中注册

---

## 许可证

本项目基于 MIT 许可证开源。详见项目根目录许可证文件。

---

> **更多细节**：项目包含完整的迭代日志（[docs/](docs/) 目录），记录了从零开始构建该项目的 27+ 次演进全过程。
