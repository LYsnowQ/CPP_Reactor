# CONFIGURATION.md — 配置事实

> 本文件是配置的**唯一现行说明**。默认值来自代码；模板值来自 `config/SQLConfig.json`，两者不是一回事。
> 核对基线：commit `221df66`（2026-11-06）。

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

> 静态文件服务会拒绝根目录下的 `config/`、`.git/` 与点文件（issue #2），因此把仓库根作为
> `resource_path` 不会再泄露数据库配置；但源码、README、`docs/` 仍会被提供下载。
> 要限制公开范围，请把专用目录作为 `resource_path`。

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
| `host` | string | `""` | `""` | ✅ | 空值启动校验失败 |
| `port` | int | `3306` | `0` | ✅ | `uint16_t`；0 启动校验失败 |
| `user` | string | `"root"` | `""` | ✅ | 空值启动校验失败 |
| `password` | string | `""` | `""` | ✅ | 不得提交真实密码 |
| `database` | string | `""` | `""` | ✅ | 空值启动校验失败 |
| `charset` | string | `utf8mb4` | `""` | ✅ | 映射到 `OPT_CHARSET_NAME` 与 `characterSetResults` |
| `connectTimeoutMs` | int | `5000` | `0` | ✅ | 传给驱动时除以 1000（秒），`<1000` 会变成 0 |
| `readTimeoutMs` | int | `5000` | `0` | ✅ | 同上 |
| `writeTimeoutMs` | int | `5000` | `0` | ✅ | 同上 |
| `pingBeforeUse` | bool | `true` | `false` | ✅ | 传入 `ThreadLocalConnOptions` |
| `maxConnLifetimeMs` | int | `1800000` | `0` | ✅ | 传入 `ThreadLocalConnOptions` |
| `maxConnIdleMs` | int | `60000` | `0` | ✅ | 传入 `ThreadLocalConnOptions`，且必须 ≤ lifetime |
| `readOnly` | bool | `true` | `false` | ✅ | HTTP 只读护栏：`true` 仅允许单条 SELECT/SHOW/DESCRIBE/EXPLAIN |

历史字段 `driver`、`connMode`、`reconnectMaxAttempts`、`reconnectBackoffMs`、`initSqls`、
`slowQueryMs`、`enableSqlLog` 从未真正生效，已从模型与模板中移除。旧配置文件里的这些键
会被 `nlohmann::json` 忽略，不会导致启动失败。

### 启动校验

`main.cpp` 在 `--enable-sql` 时调用 `loadSqlConfigFromFileOrThrow()`，它会读取 JSON 并执行
`SqlConfig::validate()`；校验失败直接退出，错误形如：

```text
服务器初始化/运行异常: 配置校验失败: host 不能为空
```

校验规则：`host`/`user`/`database`/`charset` 非空，`port > 0`，三组超时 `> 0`，
`maxConnLifetimeMs`/`maxConnIdleMs > 0` 且 idle ≤ lifetime。未开启 SQL 时不读取、不校验配置。

## 4. Docker 环境变量

`docker-entrypoint.sh` 用以下变量重写 `/app/config/SQLConfig.json`：

| 变量 | entrypoint 默认 | compose 传入 |
|------|----------------|-------------|
| `MYSQL_HOST` | `mysql` | `mysql` |
| `MYSQL_PORT` | `3306` | `3306` |
| `MYSQL_USER` | `root` | `root` |
| `MYSQL_PASSWORD` | `root` | `root` |
| `MYSQL_DATABASE` | `test` | `test` |

生成的配置包含当前全部字段，因此容器内行为与本机一致。
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
