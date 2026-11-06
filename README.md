<div align="center">

# CPPReactor

高性能 C++20 Reactor 模式 HTTP 服务器（学习项目）

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](#许可证)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20WSL2-lightgrey.svg)](#环境要求)
[![CI](https://github.com/LYsnowQ/CPP_Reactor/actions/workflows/ci.yml/badge.svg)](https://github.com/LYsnowQ/CPP_Reactor/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-1.2.0-informational.svg)](./CHANGELOG.md)

</div>

> **定位声明**
>
> 本项目是个人 C++ 网络编程学习项目，用于实践 Reactor 多线程模型、HTTP 协议解析、
> 连接生命周期管理和 MySQL 异步查询。**它不是生产级服务器**：
> 没有 TLS、没有鉴权，`/sql` 端点默认关闭；显式开启后默认只允许只读语句，
> 但服务整体仍不适合公网。请不要把本服务直接暴露到公网。详见[安全边界](#安全边界)与[已知问题](./docs/plan/BACKLOG.md)。

## 目录

- [项目简介](#项目简介)
- [核心特性](#核心特性)
- [架构概览](#架构概览)
- [环境要求](#环境要求)
- [快速开始](#快速开始)
- [配置](#配置)
- [HTTP 接口](#http-接口)
- [测试与压测](#测试与压测)
- [Docker 部署](#docker-部署)
- [项目结构](#项目结构)
- [性能基线](#性能基线)
- [文档导航](#文档导航)
- [版本与许可](#版本与许可)

---

## 项目简介

CPPReactor 从零实现了一个 C++20 的 HTTP/1.1 服务器：主线程 `accept`，多个 IO 线程各自运行
`EventLoop`，事件分发后端可在 **epoll / poll / select** 之间通过命令行参数切换；业务侧提供
静态文件服务、目录索引、健康检查，以及一个基于 MySQL Connector/C++ 的异步 SQL 查询演示端点。

项目当前版本 **1.2.0**（2026-06-19），已走完从 C 风格原型到 C++20 分层的演进，版本记录见
[CHANGELOG.md](./CHANGELOG.md)，每次迭代的技术记录见 [docs/iterations/](./docs/iterations/README.md)。

### 适用范围

| 适合 | 不适合 |
|------|--------|
| 学习 Reactor、epoll、HTTP 增量解析 | 生产环境、公网部署 |
| 作为 C++ 网络编程的参考实现 | 需要 TLS / 鉴权 / 限流的场景 |
| 压测方法论与性能对比实验 | 大文件传输、流式响应（当前整文件读入内存） |

---

## 核心特性

| 特性 | 说明 | 当前限制 |
|------|------|----------|
| **Reactor 多线程模型** | 主线程 accept，多 IO 线程事件循环，跨线程 `post()` 回调 | 无优雅停机信号处理 |
| **多事件分发后端** | epoll / poll / select 运行时切换 | 默认 epoll |
| **HTTP/1.1 解析** | 请求行、请求头、`Content-Length` Body，支持半包增量解析 | 不支持 `chunked`、不支持 HTTP/2 |
| **Keep-Alive** | 请求数上限、空闲超时、`Connection` 头协商 | — |
| **静态文件服务** | 目录索引、MIME 推断、`/healthz`、路径穿越防护 | 整文件读入内存，无 Range/分片传输 |
| **MySQL 查询（演示）** | 线程本地单连接、异步执行、结果转 JSON、跨线程回写 | 默认关闭，需 `--enable-sql`；只读模式有语句护栏；无鉴权 |
| **统一状态码** | `StatusCode` 贯穿事件循环、分发器与启动链路 | — |
| **可观测性** | 原子计数器 + 每 5 秒窗口日志（QPS、连接数、字节数、延迟） | 无 `/metrics` 导出端点 |
| **日志** | spdlog，控制台输出 | 级别在代码中固定为 `debug`，未做可配置 |
| **JSON 配置** | nlohmann_json 读取 `config/SQLConfig.json` | 部分字段只解析未生效，见[配置文档](./docs/architecture/CONFIGURATION.md) |
| **容器化** | Dockerfile + docker-compose（MySQL 8.0） | 容器内以 root 运行，测试数据需手动注入 |
| **测试与压测** | 冒烟、HTTP 边界用例、wrk 压测脚本（静态 / Echo / SQL） | 无自动化数据库集成测试 |

---

## 架构概览

```
Client
  │
  ▼
TcpServer（主线程 accept）
  │  round-robin
  ▼
IOThreadPool ──► EventLoop × N ──► Dispatcher（epoll / poll / select）
                                      │
                                      ▼
                              Channel（fd + 回调）
                                      │
                                      ▼
                              TcpConnection（解析 / 写回 / Keep-Alive）
                                      │
                                      ▼
                              HttpRouter ──► /query,/sql → SqlHandler
                                      │            └─► SqlExecutor（后台线程）
                                      └─► 其他路径 → StaticFileHandler
```

线程分工与模块职责见 [docs/architecture/ARCHITECTURE.md](./docs/architecture/ARCHITECTURE.md)。

---

## 环境要求

| 项目 | 要求 | 说明 |
|------|------|------|
| 操作系统 | Linux / WSL2 | 代码使用 `epoll`、`unistd.h` 等，**不支持 Windows 原生编译** |
| 编译器 | GCC 11+ | 需要 C++20 |
| 构建 | GNU Make 4.x（推荐）或 CMake 3.16+ | 两套构建均已提供 |
| MySQL | 5.7 / 8.0（可选） | 只有使用 `/sql` 时需要 |
| 测试 | curl、Python 3.10+、wrk | `run_http_cases.py` 只用标准库；压测需要 wrk |

所有 C++ 依赖（spdlog、nlohmann_json、MySQL Connector/C++）已内嵌在 `third_party/`，
clone 后无需 Conan、无需安装系统开发包。

---

## 快速开始

```bash
# 1. 编译（产出 build/main_run 与 build/main_echo）
make clean && make -j"$(nproc)"

# 2. 启动：<port> <静态资源根目录> [dispatcher] [threads] [conn_mode] ...
./build/main_run 8080 . epoll 4 keepalive 100 10000

# 3. 验证
curl -s http://127.0.0.1:8080/healthz          # -> ok
bash tests/test_smoke.sh http://127.0.0.1:8080/

# 4. HTTP 协议边界用例（仅标准库，无需虚拟环境）
python3 tests/run_http_cases.py 127.0.0.1 8080
```

> `resource_path` 会在启动时 `chdir`，因此静态资源根目录是相对该参数的；
> `config/SQLConfig.json` 则从**启动时的工作目录**解析。二者语义不同，见
> [配置说明](./docs/architecture/CONFIGURATION.md#路径语义)。

### CMake 构建（可选）

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j"$(nproc)"
# 产物：build-cmake/main_run、build-cmake/main_echo
```

未指定 `CMAKE_BUILD_TYPE` 时默认使用 `Release`。CMake 与 Make 使用同一份源码和 `third_party/`。

### 使用 MySQL（可选）

SQL 端点默认**不注册**，普通静态服务不需要数据库，也不会读取 SQL 配置。

1. 编辑 `config/SQLConfig.json`，至少填写 `host`、`user`、`password`、`database`；
2. 启动时追加 `--enable-sql`：
   ```bash
   ./build/main_run 8080 . epoll 4 keepalive 100 10000 --enable-sql
   ```
3. 访问 `http://127.0.0.1:8080/sql?sql=SELECT%201`。

> `readOnly: true`（模板默认）时只允许单条 `SELECT` / `SHOW` / `DESCRIBE` / `EXPLAIN`；
> 写语句返回 403。设置 `readOnly: false` 会允许任意 SQL，启动日志会给出警告。
>
> ⚠️ 当前数据库不可达时，`/sql` 请求会**一直挂起**而不是返回错误（已记录为
> [BACKLOG B-03](./docs/plan/BACKLOG.md) / issue [#3](https://github.com/LYsnowQ/CPP_Reactor/issues/3)）。

---

## 配置

完整的配置面（命令行参数、JSON 字段、环境变量、路径语义与“解析了但未生效”的字段）见
[docs/architecture/CONFIGURATION.md](./docs/architecture/CONFIGURATION.md)。

命令行参数：

```bash
./build/main_run <port> <resource_path> [dispatcher] [threads] [conn_mode] \
                 [keepalive_max_reqs] [keepalive_idle_ms] [--enable-sql]
```

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `port` | 必填 | 监听端口 |
| `resource_path` | 必填 | 静态资源根目录（启动时 chdir） |
| `dispatcher` | `epoll` | `epoll` / `poll` / `select` |
| `threads` | `4` | IO 工作线程数 |
| `conn_mode` | `close` | `close` / `keepalive` |
| `keepalive_max_reqs` | `100` | 单连接最大请求数 |
| `keepalive_idle_ms` | `10000` | 空闲超时（毫秒） |
| `--enable-sql` | 关闭 | 注册 `/query`、`/sql`，并在启动时读取 `config/SQLConfig.json` |

SQL 配置文件为 `config/SQLConfig.json`，仓库内是**空凭据模板**（`host`/`database` 为空）。
不开启 `--enable-sql` 时不会读取它；开启后配置错误会在使用时失败（启动校验见
[BACKLOG B-04](./docs/plan/BACKLOG.md)）。

---

## HTTP 接口

| 方法 | 路径 | 行为 |
|------|------|------|
| GET | `/healthz` | 返回 `ok\n`，200 |
| GET | `/query?sql=...` | 仅 `--enable-sql` 时注册；异步执行 SQL；400/403/405/500 |
| GET | `/sql?sql=...` | 与 `/query` 相同；未开启时落到静态处理器返回 404 |
| GET | 其他路径 | 静态文件或目录索引；403/404/405 |
| * | 其他方法 | 静态处理器返回 405 |

请求体上限 `1 MB`（超出返回 413），解析等待超时 `5 s`（408）。参数、错误语义与示例见
[docs/architecture/HTTP-API.md](./docs/architecture/HTTP-API.md)。

### 安全边界

- `/sql` 默认关闭；追加 `--enable-sql` 才会注册，且仍**没有鉴权**；
- `readOnly: true`（模板默认）时只允许单条 `SELECT` / `SHOW` / `DESCRIBE` / `EXPLAIN`，
  写语句返回 403；这是 HTTP 层的启发式护栏，不是完整 SQL 解析器；
- `readOnly: false` 会允许任意 SQL，只应在完全可信的本机环境使用；
- 静态文件服务拒绝根目录下的 `config/`、`.git/` 与任意以 `.` 开头的路径（issue
  [#2](https://github.com/LYsnowQ/CPP_Reactor/issues/2)），但根目录中的源码/文档仍可被读取，
  建议用独立的 `public/` 目录作为静态根；
- 没有认证、授权、限流、TLS；
- 结论：只在本机或隔离网络中使用，**不要暴露到公网**。

配置泄露与只读护栏分别由 [#2](https://github.com/LYsnowQ/CPP_Reactor/issues/2)、
[#1](https://github.com/LYsnowQ/CPP_Reactor/issues/1) 处理。

---

## 测试与压测

```bash
# 冒烟
bash tests/test_smoke.sh http://127.0.0.1:8080/

# HTTP 边界（标准库，无需 venv）
python3 tests/run_http_cases.py 127.0.0.1 8080

# SQL 端点安全（默认关闭、只读护栏；不需要真实数据库）
bash tests/test_sql_security.sh

# 静态文件压测（M1-M5 / 梯度）
bash tests/bench.sh static M
bash tests/bench.sh static GRADIENT

# Echo 基准（框架纯 IO 上限）
bash tests/bench.sh echo ECHO

# SQL 链路验证（需数据库）
bash tests/bench_sql.sh . S

# 数据注入（首次）
bash tests/setup.sh
tests/.venv/bin/python3 tests/seed_data.py --host 127.0.0.1 --user root --password '***' --database test
```

详细说明与档位定义见 [tests/README.md](./tests/README.md)，历史数据见
[tests/BENCHMARK_REPORT.md](./tests/BENCHMARK_REPORT.md)。
CI 会执行编译、冒烟与 HTTP 边界用例，配置见 [.github/workflows/ci.yml](./.github/workflows/ci.yml)。

---

## Docker 部署

```bash
docker compose up -d --build
curl http://localhost:8080/healthz
docker compose logs -f app
docker compose down
```

容器启动时 `docker-entrypoint.sh` 会用环境变量生成 `config/SQLConfig.json`：

| 变量 | 默认值 |
|------|--------|
| `MYSQL_HOST` | `mysql` |
| `MYSQL_PORT` | `3306` |
| `MYSQL_USER` | `root` |
| `MYSQL_PASSWORD` | `root` |
| `MYSQL_DATABASE` | `test` |

> compose 只负责拉起 MySQL 与应用，**不会自动注入测试数据**；需要时在启动后按
> [tests/README.md](./tests/README.md#执行顺序) 手动执行 `seed_data.py`。
> 容器以 root 运行，仅适合本机演示。

---

## 项目结构

```
CPPReactor/
├── src/
│   ├── app/            main.cpp（HTTP 服务）、main_echo.cpp（Echo 基准）
│   ├── core/           EventLoop、Buffer、Dispatcher 及三种后端
│   ├── net/            TcpServer、TcpConnection、IOThreadPool、Channel
│   ├── protocol/       HttpRequest、HttpResponse、HttpRouter
│   ├── handler/        IHttpHandler、StaticFileHandler、SqlHandler
│   ├── persistence/    SqlConfig、MySqlConnection、SqlExecutor、事务、线程本地连接
│   ├── repository/sql/ SqlRepository、SqlTask
│   ├── observability/  Metrics
│   └── utils/          JsonConfigLoader、StringUtils
├── include/            与 src/ 同构的头文件
├── third_party/        spdlog、nlohmann_json、mysql-cppconn（自包含）
├── config/             SQLConfig.json（空凭据模板）
├── tests/              冒烟 / 边界 / 压测 / 数据注入
├── docs/               规范、架构、决策、迭代、历史、计划、审查
├── scripts/            check_docs.py（文档链接检查）
├── .github/            CI、Issue/PR 模板
├── AGENTS.md           AI/协作者入口
├── CHANGELOG.md        版本记录
├── CMakeLists.txt / Makefile
├── Dockerfile / docker-compose.yml / docker-entrypoint.sh
├── compile_flags.txt   clangd 的跨平台编译参数
└── .clang-format / .clangd / .gitignore / .dockerignore
```

---

## 性能基线

2026-06-19 在 WSL2（i7-13700H / 20 线程 / 15 GiB，`-O2 -g`）测得，仅代表当时环境：

| 场景 | QPS | 说明 |
|------|-----|------|
| Echo（无 HTTP 解析） | 1,781,742 | epoll 12 线程、wrk 800 并发 |
| 静态文件（目录索引） | 60,645 | epoll 12 线程、200 并发 |
| SQL 远程查询 | ~15 | 公网 MySQL 5.7，仅验证链路，非性能数据 |

Echo 与 HTTP 的约 29 倍差距主要来自 HTTP 解析、字符串与文件 IO 等业务开销。完整数据与复现命令见
[tests/BENCHMARK_REPORT.md](./tests/BENCHMARK_REPORT.md)。

---

## 文档导航

| 入口 | 内容 |
|------|------|
| [docs/INDEX.md](./docs/INDEX.md) | 全量文档索引 |
| [docs/conventions/](./docs/conventions/README.md) | 强制规范：文档、版本、Git 工作流、测试、审查 |
| [docs/architecture/](./docs/architecture/ARCHITECTURE.md) | 架构、配置、HTTP 接口 |
| [docs/decisions/](./docs/decisions/README.md) | 架构决策记录（ADR） |
| [docs/iterations/](./docs/iterations/README.md) | 29 次迭代的历史记录（仅供追溯） |
| [docs/history/PROJECT-LOG.md](./docs/history/PROJECT-LOG.md) | 原始项目日志（0.1.0–1.2.0） |
| [docs/plan/BACKLOG.md](./docs/plan/BACKLOG.md) | 已知问题与排期 |
| [docs/reviews/2026-11-06-NORMALIZATION-AUDIT.md](./docs/reviews/2026-11-06-NORMALIZATION-AUDIT.md) | 本次文档/配置规范化的审计与方案 |
| [AGENTS.md](./AGENTS.md) | AI/协作者进入仓库的第一步 |

---

## 版本与许可

当前版本 **1.2.0**。版本号遵循 SemVer，唯一事实来源是 git tag + [CHANGELOG.md](./CHANGELOG.md)，
规则见 [docs/conventions/VERSIONING.md](./docs/conventions/VERSIONING.md)。

本项目基于 MIT 许可证开源，详见 [LICENSE](./LICENSE)。
