# CONFIGURATION.md — 配置事实

> 本文件是配置的**唯一现行说明**。默认值来自代码；模板值来自 `config/SQLConfig.json`，两者不是一回事。
> 核对基线：commit `483b7b1`（2026-11-06）。

## 1. 路径语义（最容易踩错）

| 项 | 解析方式 | 含义 |
|----|---------|------|
| `resource_path` | 启动时 `chdir(argv[2])`（失败即退出） | 静态文件根目录，也是进程工作目录 |
| `config/SQLConfig.json` | `启动时工作目录 / config / SQLConfig.json` | **在 chdir 之前记录的原工作目录**下查找 |
| 日志 | 仅控制台 | 无文件输出 |
| Docker | `WORKDIR=/app`，CMD `./main_run 8080 /app` | 原工作目录与资源根目录都是 `/app` |

因此：

```bash
# 正确：在仓库根目录启动，config/ 能被找到
./build/main_run 8080 . epoll 4 keepalive

# 错误：在 /tmp 启动，虽然资源目录是项目根，但配置会去 /tmp/config 找
cd /tmp && /path/to/build/main_run 8080 /path/to/CPPReactor epoll 4
```

## 2. 命令行参数

```bash
./build/main_run <port> <resource_path> [dispatcher] [threads] [conn_mode] \
                 [keepalive_max_reqs] [keepalive_idle_ms] [--enable-sql]
```

| 位置 | 参数 | 代码默认 | 解析方式 | 说明 |
|------|------|---------|---------|------|
| 1 | `port` | 必填 | `atoi` → `uint16_t` | 无范围校验；`0` 会绑定随机端口 |
| 2 | `resource_path` | 必填 | `chdir` | 失败退出 |
| 3 | `dispatcher` | `epoll` | 字符串转小写匹配 | 未知值**静默回退 epoll** |
| 4 | `threads` | `4` | `atoi` | 无校验；传 `0` 的行为未定义/未测试 |
| 5 | `conn_mode` | `close` | 字符串比较 | 只有 `keepalive` 开启，其他值等同 `close` |
| 6 | `keepalive_max_reqs` | `100` | `atoi` | 仅 keepalive 生效 |
| 7 | `keepalive_idle_ms` | `10000` | `atoi` | 仅 keepalive 生效 |
| — | `--enable-sql` | 关闭 | 布尔 flag，可出现在任意位置 | 注册 `/query`、`/sql`，并读取 SQL 配置 |

位置参数少于 2 个时打印 Usage 并返回 -1；`--help` / `-h` 返回 0。暂无 `--version`。

## 3. SQL 配置字段

`config/SQLConfig.json` **仅在 `--enable-sql` 时读取**。所有键都是必填：反序列化使用
`j.at(key)`，缺键会抛异常。仓库内是空凭据模板，不开启 SQL 时不会读取，也不影响静态服务。

| 键 | 类型 | 模板值 | 结构体默认 | 是否生效 | 说明 |
|----|------|--------|-----------|---------|------|
| `driver` | string | `mysql_connector_cpp` | `Unknown` | ❌ 解析后未使用 | 连接固定走 JDBC 驱动实例 |
| `host` | string | `""` | `""` | ✅ | 空值导致连接失败 |
| `port` | int | `3306` | `0` | ✅ | `uint16_t` |
| `user` | string | `"root"` | `""` | ✅ | |
| `password` | string | `""` | `""` | ✅ | 不得提交真实密码 |
| `database` | string | `""` | `""` | ✅ | 空值时连接不带 schema |
| `charset` | string | `utf8mb4` | `""` | ✅ | 映射到 `OPT_CHARSET_NAME` 与 `characterSetResults` |
| `connectTimeoutMs` | int | `5000` | `0` | ✅ | 传给驱动时除以 1000（秒），`<1000` 会变成 0 |
| `readTimeoutMs` | int | `5000` | `0` | ✅ | 同上 |
| `writeTimeoutMs` | int | `5000` | `0` | ✅ | 同上 |
| `connMode` | string | `thread_local_single_conn` | `Unknown` | ❌ 解析后未使用 | 只支持这一种连接管理方式 |
| `pingBeforeUse` | bool | `true` | `false` | ❌ 未接线 | `main.cpp` 传入的 `ThreadLocalConnOptions{}` 使用自己的默认值 |
| `reconnectMaxAttempts` | int | `3` | `0` | ❌ 未使用 | 驱动参数 `OPT_RECONNECT` 硬编码为 true |
| `reconnectBackoffMs` | int | `1000` | `0` | ❌ 未使用 | 仅 `validate` 中做非零检查 |
| `maxConnLifetimeMs` | int | `1800000` | `0` | ❌ 未接线 | 管理器实际使用 `ThreadLocalConnOptions` 的 30 分钟默认值 |
| `maxConnIdleMs` | int | `60000` | `0` | ❌ 未接线 | 管理器实际使用 60 秒默认值 |
| `initSqls` | array | `["SELECT 1"]` | 空 | ❌ 未执行 | 连接建立后不执行初始化 SQL |
| `slowQueryMs` | int | `1000` | `0` | ❌ 未使用 | 没有慢查询日志 |
| `enableSqlLog` | bool | `true` | `false` | ❌ 未使用 | SQL 日志未实现 |
| `readOnly` | bool | `true` | `false` | ✅ HTTP 只读护栏 | `true` 时仅允许单条 SELECT/SHOW/DESCRIBE/EXPLAIN；`false` 允许任意 SQL |

证据：

- `src/app/main.cpp:72-85`：使用通用 `loadJsonFileOrThrow`，并传入 `ThreadLocalConnOptions{}`；
- `src/persistence/MySqlConnection.cpp:145-180`：只消费连接级字段与三组超时；
- `src/persistence/ThreadLocalSqlConn.cpp:61-98`：使用 `ThreadLocalConnOptions` 而非 `SqlConfig`；
- `src/persistence/SqlConfig.cpp:65-150`：存在 `validate()`，但启动链路从未调用。

### 启动校验缺失

`loadSqlConfigFromFileOrThrow()`（读取 + `validate`）已经实现，但 `main.cpp` 没有使用它，
而是直接 `json → SqlConfig` 隐式转换。结果是：

- 缺键会失败（`at()`）；
- 空 `host`/`database`、`0` 端口、超时缺失等**不会**在启动时被拦住，
  服务照常启动，直到第一次 `/sql` 才失败（并且当前会挂起，见 BACKLOG B-02）。

## 4. Docker 环境变量

`docker-entrypoint.sh` 用以下变量重写 `/app/config/SQLConfig.json`：

| 变量 | entrypoint 默认 | compose 传入 |
|------|----------------|-------------|
| `MYSQL_HOST` | `mysql` | `mysql` |
| `MYSQL_PORT` | `3306` | `3306` |
| `MYSQL_USER` | `root` | `root` |
| `MYSQL_PASSWORD` | `root` | `root` |
| `MYSQL_DATABASE` | `test` | `test` |

生成的配置包含完整字段（含未生效字段），因此容器内行为与本机一致。
注意：entrypoint 会**覆盖**镜像内 `config/SQLConfig.json`，挂载自定义配置时需绕过该脚本或挂到别处。

## 5. 端口与网络

| 端口 | 用途 |
|------|------|
| 8080 | HTTP（README / Docker / CI 默认） |
| 8081 | Echo 基准（`tests/bench.sh` 使用） |
| 3306 | MySQL（compose 暴露） |

服务监听 `0.0.0.0`（`TcpServer` 绑定任意地址），没有只监听 localhost 的选项。

## 6. 修改配置的规则

1. 新增字段：更新 `SqlConfig`、`from_json`/`to_json`/`validate`、真正使用它的代码、本文件；
2. 若暂时不打算实现，必须在本文件标注 ❌ 与原因，不允许让读者以为它生效；
3. 修改默认值：同时更新本表、README 的参数表、CHANGELOG；
4. 真实凭据只放本机文件，提交前 `git diff` 自查；`config/SQLConfig.json` 保持空模板。
