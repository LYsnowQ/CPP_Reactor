# 测试与基准

## 目录结构

```
tests/
├── README.md                           # 本文件
├── BENCHMARK_REPORT.md                 # 基准压测报告（含环境信息 + 历史数据）
├── setup.sh                            # Python 虚拟环境初始化
├── requirements.txt                    # Python 依赖声明
│
├── seed_data.py                        # 数据注入（向 MySQL users 表插入 5 万行）
├── test_smoke.sh                       # 冒烟测试（curl 确认服务可用）
├── run_http_cases.py                   # HTTP 协议边界测试（合法/非法/超大包/半包）
│
├── bench_static.sh                     # 静态文件压测（wrk，多档位 S/M/L/LOCAL_MAX）
├── bench_sql.sh                        # SQL 查询压测（wrk，多模式 S/M/L/ALL）
│
├── benchmarks/                         # 压测输出目录（已 gitignore）
│   └── BENCHMARK_REPORT.md             # → 基准压测报告（汇总 + 环境信息）
│
└── bench_reports/                      # 历史报告存档（已 gitignore）
```

## 执行顺序

```bash
# 1. 编译
make clean && make -j$(nproc)

# 2. 启动服务
./main_run 8080 /tmp epoll 4 close

# 3. 冒烟测试
bash tests/test_smoke.sh http://127.0.0.1:8080/

# 4. HTTP 边界测试
tests/.venv/bin/python3 tests/run_http_cases.py 127.0.0.1 8080

# 5. 静态文件压测
bash tests/bench_static.sh . M

# 6. SQL 压测（需要已连接 MySQL + 已注入数据）
bash tests/bench_sql.sh . S

# 7. 数据注入（首次需要）
bash tests/setup.sh && tests/.venv/bin/python3 tests/seed_data.py
```

## 各文件说明

### 环境管理

| 文件 | 说明 |
|------|------|
| `setup.sh` | 创建 Python 3.12 虚拟环境到 `tests/.venv/` 并安装依赖 |
| `requirements.txt` | pip 依赖声明，当前仅 `pymysql` |

### 功能测试

| 文件 | 工具 | 用途 |
|------|------|------|
| `test_smoke.sh` | curl | 快速确认服务是否可访问，检查 HTTP 返回码 |
| `run_http_cases.py` | socket | 覆盖 413 大包、分片请求、非法请求等边缘情况 |
| `seed_data.py` | pymysql | 向 MySQL users 表插入 5 万行测试数据 |

### 性能压测

| 文件 | 工具 | 用途 |
|------|------|------|
| `bench_static.sh` | wrk | 静态文件压测（根路径/目录索引），多档位自动测试，输出 Markdown |
| `bench_sql.sh` | wrk | SQL 查询压测，多模式覆盖轻量/中等/重度查询，输出 Markdown |

### 输出目录

| 目录 | 说明 |
|------|------|
| `benchmarks/` | 压测输出（.md），已 gitignore |
| `bench_reports/` | 历史归档报告 |
| `.venv/` | Python 虚拟环境，已 gitignore |

## 压测档位说明

### bench_static.sh (PROFILE)

| 档位 | 适用场景 | 说明 |
|------|---------|------|
| S | 4C8G | 3 组用例，低并发 |
| M | 8C16G | 4 组用例，中等并发 |
| L | 16C32G | 4 组用例，高并发 |
| LOCAL_MAX | 自动适配 | 按当前机器核数自适配，最高并发 |

### bench_sql.sh (PROFILE)

| 档位 | 并发范围 | 说明 |
|------|---------|------|
| S | 1-2 wrk, 5-10 conn | 轻量：验证功能 + 基础 QPS |
| M | 2-4 wrk, 10-20 conn | 中等混合 |
| L | 4-6 wrk, 20-40 conn | 重度高并发 |
| ALL | 全量 | 覆盖所有用例 |
