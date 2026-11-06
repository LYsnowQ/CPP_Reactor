# TESTING.md — 测试与验证

## 1. 现状

| 层级 | 是否存在 | 位置 |
|------|---------|------|
| 单元测试 | ❌ 暂无 | — |
| 协议/接口端到端 | ✅ | `tests/run_http_cases.py`、`tests/test_smoke.sh` |
| 数据库链路 | ⚠️ 手动 | `tests/seed_data.py`、`tests/bench_sql.sh` |
| 性能基线 | ✅ 手动 | `tests/bench.sh`、`tests/bench_sql.sh` |
| 文档检查 | ✅ | `scripts/check_docs.py` |
| CI | ✅ | `.github/workflows/ci.yml` |

没有单元测试是当前明确的技术债（BACKLOG B-12）。在此之前，**任何“验证通过”的结论必须
说明跑的是哪一层，不能把端到端用例通过等同于全部正确**。

## 2. 提交前必须执行

```bash
# 1. 编译（零错误零警告）
make clean && make -j"$(nproc)"

# 2. 启动
./build/main_run 8080 . epoll 4 keepalive 100 10000 &

# 3. 冒烟
bash tests/test_smoke.sh http://127.0.0.1:8080/

# 4. HTTP 边界用例
python3 tests/run_http_cases.py 127.0.0.1 8080

# 5. 文档
python3 scripts/check_docs.py

# 6. 收尾
kill %1
```

CMake 路径改动还需：

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j"$(nproc)"
```

## 3. HTTP 用例覆盖范围

`tests/run_http_cases.py` 当前只有 4 个用例，覆盖：

1. 合法 `GET /` → 200；
2. 非法请求行 → 400；
3. 超过 1 MB 的请求体 → 413（客户端可能先收到 RST，用例保留已读字节）；
4. `Content-Length` Body 分片到达 → 解析完成后正常分派 → 200。

**未覆盖**：Keep-Alive 请求数上限与空闲超时、408、403 路径穿越、405、`/sql` 的 400/405/500、
并发连接回收、半关闭与错误注入。新增用例优先补这些。

## 4. 数据库验证

- 默认 `config/SQLConfig.json` 为空凭据，**不能**作为数据库验证环境；
- 本机验证步骤：填好配置 → `bash tests/setup.sh` → `seed_data.py` → 启动 → 请求 `/sql`；
- 必须记录：MySQL 版本、数据量、是否公网、查询语句；
- 数据库不可达时的错误路径目前**没有自动化用例**，且存在挂起缺陷（B-02）。修复后必须补一条
  “连接失败 → 5xx”的用例。

## 5. 压测规范

1. 客户端只用 wrk；Python 客户端数据不能作为服务器上限结论；
2. 每档至少 8 秒，记录 CPU 核数、线程数、连接数、编译选项、提交号；
3. 结论必须来自多档梯度，不用单点数字；
4. 原始输出写入 `tests/benchmarks/`（gitignore），里程碑结论才更新
   `tests/BENCHMARK_REPORT.md`；
5. 报告中注明“何时、何机器、何提交”，旧数据不得冒充当前性能；
6. 性能改动必须同时给出功能测试结果，性能数字不能替代正确性。

## 6. 验证结论怎么写

在 PR / issue 评论中使用固定格式：

```
验证环境：WSL2 Ubuntu 22.04 / g++ 11.4 / commit abc1234
执行：make clean && make -j8
结果：零错误零警告
执行：python3 tests/run_http_cases.py 127.0.0.1 8080
结果：4/4 通过
未验证：真实 MySQL 查询、Keep-Alive 超时
```

禁止只写“测试通过”。未验证的部分要显式列出。

## 7. 测试自身的约束

- 用例期望值必须来自协议/文档，而不是“当前实现碰巧返回什么”；
- 修正错误期望时，必须在提交信息里说明原期望为何错误；
- 不允许用 `|| true`、跳过、删除断言来让 CI 变绿；
- 测试失败先判断是实现问题还是用例问题，再决定改哪边。
