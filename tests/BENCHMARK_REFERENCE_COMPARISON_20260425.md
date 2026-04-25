# 压测对照与服务器预期（2026-04-25）

## 1. 本机测试环境
- OS: Linux 5.15.167.4-microsoft-standard-WSL2 (x86_64)
- CPU: Intel i7-13700H，20 vCPU（10C/20T）
- 内存: 15GiB
- 压测工具: wrk 4.1.0
- 服务模式: `main_run` + 静态目录路由，HTTP 短响应

> 说明：本机在 WSL2 下，结果会受宿主机调度与后台负载影响，建议关注区间而不是单点值。

## 2. 最新本机压测结果（本轮）
数据来源：
- `tests/benchmarks/bench_matrix_20260425_local.md`
- `tests/benchmarks/bench_gradient_20260425_local.md`

关键结论（取核心 case）：

| case | 场景 | req/sec | latency_avg | 备注 |
|---|---|---:|---|---|
| E1 | epoll, keepalive, io=1, c=50 | 6835.98 | 7.30ms | 低线程基线 |
| E2 | epoll, keepalive, io=4, c=100 | 22368.42 | 4.43ms | 中并发主力段 |
| E3 | epoll, keepalive, io=8, c=200 | 23869.16 | 6.10ms | 有 timeout=200 抖动 |
| E4 | epoll, close, io=8, c=200 | 20865.69 | 7.22ms | close 低于 keepalive |
| G3 | epoll, keepalive, io=8, c=200 | 33260.26 | 5.85ms | 同级条件下更高，说明有波动 |
| G4 | epoll, keepalive, io=8, c=400 | 28762.07 | 13.91ms | 并发升高后时延明显上升 |

## 3. 与前一轮历史结果对照（同 case）
历史基线来源：
- `tests/bench_reports/benchmark_full_20260425_130319.txt`（E1~E4）

| case | 历史 req/sec | 本轮 req/sec | 变化 |
|---|---:|---:|---:|
| E1 | 6652.53 | 6835.98 | +2.76% |
| E2 | 23959.93 | 22368.42 | -6.64% |
| E3 | 34796.02 | 23869.16 | -31.40% |
| E4 | 19839.42 | 20865.69 | +5.17% |

解读：
- E1/E4 小幅提升，E2 轻微回落，E3 明显回落且伴随 timeout，说明高并发段稳定性仍需继续收敛。
- 同样是 `epoll+keepalive+io=8`，G3 与 E3 差异较大，表明当前结果受“运行时波动”影响较大（WSL2 调度、后台负载、短时测试窗口）。

## 4. 中小公司常见服务器（非顶配）预期对照
以下为“经验预期区间”，用于制定目标，不是实测值。假设：
- 同版本代码、同请求模型（短响应、keepalive 为主）
- 同机压测（压测机与服务机分离时通常更高）
- Linux 原生环境（非 WSL）

| 档位 | 常见配置（非顶配） | 建议目标 case | 预期 req/sec 区间 | 预期平均时延 |
|---|---|---|---:|---|
| S | 4C8G 云主机 | E2 等级（c=100） | 10000 ~ 20000 | 6ms ~ 20ms |
| M | 8C16G 云主机 | E3/G3 等级（c=200） | 20000 ~ 45000 | 4ms ~ 15ms |
| L | 16C32G 通用型 | G3/G4 等级（c=200~400） | 35000 ~ 70000 | 3ms ~ 12ms |

## 5. 本机阶段性目标区间
- 当前结果对应的稳定区间可定义为：
  - `c=100`：2.0w ~ 2.5w req/sec
  - `c=200`：2.3w ~ 3.3w req/sec（优先压低 timeout）
- 基线回归阈值可定义为：
  - E2 req/sec 不低于 2.1w
  - E3 timeout 接近 0（例如 < 20）
  - E4 长期低于 E3（保持 keepalive 相对优势）

## 6. 服务器档位压测预设
- 已提供档位化脚本：`scripts/bench_server_profiles.sh`
- 支持四种 profile：
  - `S`：近似 4C8G
  - `M`：近似 8C16G
  - `L`：近似 16C32G
  - `LOCAL_MAX`：按本机 `nproc` 自适应上限探索
- 示例命令：
  - `bash scripts/bench_server_profiles.sh /project/CPPReactor S`
  - `bash scripts/bench_server_profiles.sh /project/CPPReactor M`
  - `bash scripts/bench_server_profiles.sh /project/CPPReactor L`
  - `bash scripts/bench_server_profiles.sh /project/CPPReactor LOCAL_MAX /project/CPPReactor/tests/benchmarks/bench_profile_local_max.md 15s`

## 7. 产物位置（已统一到 tests）
- 矩阵报告：`tests/benchmarks/bench_matrix_20260425_local.md`
- 梯度报告：`tests/benchmarks/bench_gradient_20260425_local.md`
- 历史全量 txt：`tests/bench_reports/benchmark_full_20260425_130319.txt`
- 本说明：`tests/BENCHMARK_REFERENCE_COMPARISON_20260425.md`
