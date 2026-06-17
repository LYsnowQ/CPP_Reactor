# Iteration 28 - 第三方库自包含改造：内嵌 MySQL Connector 与移除 Conan

## 本轮目标
- 将 MySQL Connector/C++（头文件 + .so）集成到 `third_party/` 目录
- 将 nlohmann_json 从 Conan 管理改为内嵌到 `third_party/`
- 移除 Conan 构建依赖，使项目 clone 后即可 `make` 编译
- 同步更新 Docker 构建流程

## 背景

项目此前依赖管理混合：
- **spdlog** — 已内嵌 `third_party/spdlog/` ✅
- **nlohmann_json** — Conan 管理，Makefile 靠搜索 `~/.conan2` 缓存定位
- **MySQL Connector/C++** — 系统 apt 包 `libmysqlcppconn-dev`，编译时从 `/usr/include/mysql-cppconn` 取头文件、从系统 lib 目录链接 `.so`

传递项目时需引导用户装 conan 或 apt 包，不够自包含。

## 改动清单

### 新增文件/目录

```
third_party/
├── nlohmann_json/nlohmann/   (从 Conan 缓存复制，multi-header 全量)
└── mysql-cppconn/
    ├── include/jdbc/          (从 /usr/include/mysql-cppconn 复制)
    ├── include/mysql/
    ├── include/mysqlx/
    └── lib/                   (从 /usr/lib/x86_64-linux-gnu 复制 .so 及 symlink)
        ├── libmysqlcppconn.so -> libmysqlcppconn.so.10
        ├── libmysqlcppconn.so.10 -> libmysqlcppconn.so.10.9.7.0
        ├── libmysqlcppconn.so.10.9.7.0
        ├── libmysqlcppconnx.so -> libmysqlcppconnx.so.2
        ├── libmysqlcppconnx.so.2 -> libmysqlcppconnx.so.2.9.7.0
        ├── libmysqlcppconnx.so.2.9.7.0
        └── libmysqlcppconn8.so.2 -> libmysqlcppconnx.so.2
```

### 修改文件

| 文件 | 改动 |
|------|------|
| `CMakeLists.txt` | 移除 Conan CMakeDeps、`find_package(spdlog)`、`find_package(nlohmann_json)`、`find_path/find_library` 系统 MySQL 查找 → 直接用 `target_include_directories` 指向 `third_party/`，`target_link_libraries` 直接指定 `.so` 路径；通过 `BUILD_RPATH/INSTALL_RPATH` 确保运行时定位 |
| `Makefile` | 移除 `CONAN_NLOHMANN` shell 查找逻辑，`CPPFLAGS` 固定指向 `./third_party/nlohmann_json` 和 `./third_party/mysql-cppconn/include`；新增 `-L./third_party/mysql-cppconn/lib` 和 `-Wl,-rpath,'$ORIGIN/third_party/mysql-cppconn/lib'` |
| `.clangd` | include 路径从 `/home/ghl/.conan2/...` 和 `/usr/include/mysql-cppconn` 改为项目内路径 |
| `Dockerfile` | Stage 1 不再 apt 安装 `libmysqlcppconn-dev` 和 `nlohmann-json3-dev`（仅保留 `g++`、`make`、`ca-certificates`）；先 COPY `third_party/` 再 COPY 源码，利用 Docker 缓存层；Stage 2 从构建阶段的 `third_party/` 复制 `.so` 到 `/app/lib/`，通过 `LD_LIBRARY_PATH` 定位 |
| `.dockerignore` | 移除已删除的 `conanfile.py` |
| `README.md` | 运行时依赖表改为"无需额外安装"；快速开始移除 `apt install` 步骤；移除"CMake + Conan"构建章节；项目结构补充 `nlohmann_json` 和 `mysql-cppconn` |

### 删除文件

| 文件 | 原因 |
|------|------|
| `conanfile.py` | 不再需要 Conan 管理依赖 |

## 构建验证

```bash
make clean && make -j$(nproc)
# 结果: 25 个 .o → 链接成功，0 warning 0 error
# 二进制: main_run (18M, ELF x86-64, RUNPATH=$ORIGIN/third_party/mysql-cppconn/lib)
```

## Docker 构建变化

**改造前** Dockerfile 需要 apt 安装 2 个库 + 从系统 lib 复制 `.so`。
**改造后** Dockerfile 只需 COPY `third_party/` 目录，不再需要 apt 下载任何 C++ 依赖库，构建更快、镜像更小。

## 后续注意事项

- **库版本更新**：nlohmann_json 和 MySQL Connector/C++ 需手动替换 `third_party/` 中的文件
- **平台迁移**：MySQL Connector/C++ 的 `.so` 是 x86_64 Linux 的，迁移到 ARM 或其他平台需替换对应的 `.so`
- **备份位于**：`/tmp/CPPReactor_backup/`
