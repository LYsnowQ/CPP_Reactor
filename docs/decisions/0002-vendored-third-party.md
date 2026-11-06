# 0002 — 第三方库自包含于 `third_party/`

- 状态：accepted
- 日期：2026-06-17（ITERATION 28）；回溯整理于 2026-11-06
- 相关：`third_party/`、`Makefile`、`CMakeLists.txt`、`Dockerfile`

## 背景

早期依赖 Conan（spdlog、nlohmann_json）与系统包（`libmysqlcppconn-dev`）。问题是：

- clone 后需要先装 Conan / 配 remote / 命中缓存，否则无法编译；
- 系统包版本随发行版变化，MySQL Connector 的库名（`mysqlcppconn` / `mysqlx`）在不同环境不一致；
- 学习项目的读者通常只想 `make` 一次就能跑。

## 决策

把 spdlog（header-only）、nlohmann_json（header-only）、MySQL Connector/C++（头文件 + `.so`）
全部放入 `third_party/`，删除 `conanfile.py`，构建系统直接引用目录内路径。

## 理由

- `clone → make` 闭环，降低接手成本；
- 版本固定，避免“我这里能编译”的环境差异；
- Docker 构建层可缓存 `third_party/`，不必每次装依赖。

## 后果

正面：

- 无需 Conan、无需 apt 开发包；
- 依赖版本可追溯（随仓库提交）；
- Dockerfile 精简，只装 `g++`/`make`。

负面：

- 仓库包含 Linux x86_64 的 `.so` 与 symlink，**不可跨平台/跨架构直接使用**；
- 依赖升级需要手工替换并验证，容易长期停留在旧版本；
- 二进制不可审计（来源、构建方式），存在供应链风险；
- 仓库体积增大。

## 替代方案

| 方案 | 放弃原因 |
|------|---------|
| Conan / vcpkg | 需要额外工具链与网络，破坏“clone 即编译” |
| 系统 apt 包 | 版本不可控，库名不稳定 |
| 源码子模块 + 自行构建 | 构建时间明显增加，学习项目收益低 |

## 重新评估条件

- 需要支持 ARM Linux / macOS / Windows；
- 升级 MySQL Connector 或出现安全公告；
- 需要可审计的依赖来源（例如 CI 中校验 hash）。
