# 测试蓝图（第一步）

## 目标
- 提供“可立即执行”的基础测试闭环：冒烟、协议用例、基线压测。
- 让后续每轮优化都可以做前后对比，不再靠体感判断。

## 目录说明
- `tests/run_http_cases.py`
  - 原始 HTTP 协议用例运行器（无第三方依赖）。
  - 覆盖：合法请求、非法请求、超大包、半包请求。
- `tests/BENCHMARK_REFERENCE_COMPARISON_20260425.md`
  - 基准对照说明（环境、指标口径、目标区间）。
- `scripts/test_smoke.sh`
  - 基础冒烟（`curl`），快速确认服务可用。
- `scripts/bench_server_profiles.sh`
  - 按服务器档位（S/M/L/LOCAL_MAX）执行参数化压测并输出 Markdown。

## 执行顺序（建议）
1. 先启动服务：`./main_run <port> <resource_path> [dispatcher] [threads]`
2. 运行冒烟：`./scripts/test_smoke.sh http://127.0.0.1:8080/`
3. 运行协议用例：`python3 tests/run_http_cases.py 127.0.0.1 8080`
4. 运行性能对照：`bash scripts/bench_server_profiles.sh /project/CPPReactor M`

## 结果解读
- 冒烟失败：优先排查服务是否启动、端口/资源目录是否正确。
- 协议用例失败：查看具体用例名称定位解析链路问题。
- 性能对照：结合服务端 `[metrics]` 日志与压测输出一起分析，不只看 QPS。
