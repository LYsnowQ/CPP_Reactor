# HTTP-API.md — 接口契约

> 适用范围：`main_run`（HTTP 服务器）。`main_echo` 不是 HTTP 服务，只回固定 200 空响应。
> 核对基线：commit `483b7b1`。接口语义变化时必须同步本文件与 CHANGELOG。

## 1. 路由总表

| 方法 | 路径 | 处理器 | 成功 | 失败 |
|------|------|--------|------|------|
| GET | `/healthz` | StaticFileHandler | 200 `ok\n` | 405（非 GET） |
| GET | `/query` | SqlHandler | 200 JSON | 400 / 405 / 500 / 挂起（见 §4） |
| GET | `/sql` | SqlHandler | 200 JSON | 同上 |
| GET | 其他路径 | StaticFileHandler | 200 文件或目录索引 | 403 / 404 / 405 / 500 |
| 其他方法 | 任意 | 路由 → 静态处理器 | — | 405 |

路由匹配顺序：先精确匹配（`/query`、`/sql`），再前缀匹配（`/` → 静态文件），最后 404。
`HttpRouter` 支持 `:param` 路径参数，但当前内置路由没有使用。

## 2. 通用协议行为

| 项 | 行为 |
|----|------|
| HTTP 版本 | 解析 HTTP/1.1 请求行；响应统一 `HTTP/1.1` |
| 请求体 | 仅支持 `Content-Length`；上限 **1 MB**，超出 413 |
| `Transfer-Encoding: chunked` | 不支持 |
| 解析超时 | 请求未收全且等待超过 **5 秒** → 408 |
| Keep-Alive | `conn_mode=keepalive` 时启用；最大请求数默认 100，空闲超时默认 10 s |
| `Connection: close` | 请求头要求关闭时，本次响应后关闭 |
| 响应缓冲 | 完整响应在内存中组装后写出；无流式/分块响应 |
| 错误响应体 | 纯文本（除 SQL 处理器返回 JSON） |

## 3. 静态文件

- 仅 `GET`（大小写不敏感地接受 `get`）；其他方法返回 405，正文 `Only GET is supported.\n`。
- 目录：返回 HTML 目录索引，目录在前、文件在后，各自按名称排序；**不会**自动寻找 `index.html`。
- 文件：按扩展名推断 `Content-Type`，未知类型回退为 `application/octet-stream`。
- 路径穿越：规范化后必须仍在根目录内，否则 403。
- 不存在：404。
- 不发送 `Last-Modified`、`ETag`、`Range`，不支持断点续传与缓存协商。
- `/healthz` 在文件系统查找之前直接返回 `ok\n`。

> ⚠️ **静态根目录包含什么，就能下载什么。** 用仓库根目录（`.`）启动时，
> `/config/SQLConfig.json`、`/README.md`、源码与 `third_party/` 都可被直接下载。
> 一旦在配置里填入真实数据库密码，密码就会通过 HTTP 暴露。见 §6。

## 4. SQL 查询接口

### 请求

```
GET /sql?sql=<percent-encoded SQL>
GET /query?sql=<percent-encoded SQL>
```

- 仅 `GET`；其他方法 405，正文 `{"error":"Method not allowed"}`。
- 缺少 `sql` 参数或值为空：400，`{"error":"Missing 'sql' query parameter"}`。
- `sql` 值先做 URL 解码；参数名必须为 `sql`，其他参数忽略。
- 请求在 SQL 工作线程执行，HTTP 连接保持挂起直到结果回写。

### 成功响应

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8

[
  [1, "alice"],
  [2, "bob"]
]
```

- 结果格式为**二维数组**（不是对象数组），没有列名；
- 整数 → number，浮点 → number，字符串/日期 → string，NULL → `null`；
- JSON 使用 2 空格缩进。

### 失败响应

```json
{"error":"SQL query failed","message":"<驱动返回的原始错误>"}
```

- 状态码 500；
- `message` 会透出数据库错误信息，属于信息泄露面，仅限本机使用。

### 已知缺陷（未修复）

**数据库不可达时请求不会返回错误，而是一直挂起。** 原因链：

1. `ThreadLocalSingleConn::ensureConnected_()` 连接失败抛异常；
2. `SqlExecutor::run_()` 捕获后只写 `spdlog::warn`，任务结束；
3. 没有任何代码向连接回写响应，`async = true` 的连接永远等待。

复现：使用空 `host` 的模板配置启动，`curl --max-time 8 'http://127.0.0.1:8080/sql?sql=SELECT%201'`
超时，服务端日志出现 `SqlExecutor job failed: sql connect failed`。
登记为 [BACKLOG B-02](../plan/BACKLOG.md)。

## 5. 状态码汇总

| 码 | 触发条件 |
|----|---------|
| 200 | 正常文件/目录/健康检查/SQL 成功 |
| 400 | 请求行或头部非法；SQL 参数缺失 |
| 403 | 静态路径越界 |
| 404 | 路径无匹配 |
| 405 | 非 GET 访问静态/SQL 接口 |
| 408 | 请求未收全超时 |
| 413 | `Content-Length` 超过 1 MB |
| 500 | 文件读取失败、SQL 执行失败 |

## 6. 安全边界（必须遵守）

| 风险 | 现状 | 要求 |
|------|------|------|
| 任意 SQL 执行 | `/sql` 接受任意语句，无鉴权、无白名单 | 只在本机/隔离网络使用；不要公网暴露 |
| 凭据泄露 | 静态根可下载 `config/SQLConfig.json` | 填真实凭据时不要把仓库根作为静态根；或用独立目录 |
| `readOnly` 假象 | 配置项被解析但未强制执行 | 不要依赖它做保护 |
| 错误信息泄露 | SQL 500 返回驱动原始错误 | 仅本机 |
| 无 TLS / 无速率限制 | — | 不接公网 |

修复方向记录在 [BACKLOG B-01](../plan/BACKLOG.md)：默认拒绝 `/sql` 写语句、配置移出静态根、
或增加 `--no-sql` 开关。
