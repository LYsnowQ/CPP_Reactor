#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${1:-.}"
PROFILE="${2:-S}"
OUT_MD="${3:-${ROOT_DIR}/tests/benchmarks/bench_sql_${PROFILE}_latest.md}"
DURATION="${4:-15s}"

if ! command -v wrk >/dev/null 2>&1; then
    echo "[sql-bench][FAIL] wrk not found"
    exit 1
fi

cd "${ROOT_DIR}"
mkdir -p "$(dirname "${OUT_MD}")"

# ── SQL 查询用例 ──
# 每个用例: "名称   wrk线程  并发数  URL"
declare -a QUERIES

case "${PROFILE}" in
    S)
        QUERIES=(
            "light_count     1   5   /query?sql=SELECT+count(*)+FROM+users"
            "light_lookup    1   5   /query?sql=SELECT+*+FROM+users+WHERE+name='user_000001'"
            "light_limit100  2  10   /query?sql=SELECT+*+FROM+users+LIMIT+100"
        )
        ;;
    M)
        QUERIES=(
            "mid_count       2  10   /query?sql=SELECT+count(*)+FROM+users"
            "mid_lookup      2  10   /query?sql=SELECT+*+FROM+users+WHERE+name='user_005000'"
            "mid_limit100    4  20   /query?sql=SELECT+*+FROM+users+LIMIT+100"
            "mid_limit1000   4  20   /query?sql=SELECT+*+FROM+users+LIMIT+1000"
        )
        ;;
    L)
        QUERIES=(
            "heavy_count      4  20   /query?sql=SELECT+count(*)+FROM+users"
            "heavy_lookup     4  20   /query?sql=SELECT+*+FROM+users+WHERE+name='user_030000'"
            "heavy_limit100   6  40   /query?sql=SELECT+*+FROM+users+LIMIT+100"
            "heavy_limit5000  6  40   /query?sql=SELECT+*+FROM+users+LIMIT+5000"
        )
        ;;
    ALL)
        QUERIES=(
            "light_count     1   5   /query?sql=SELECT+count(*)+FROM+users"
            "light_lookup    1   5   /query?sql=SELECT+*+FROM+users+WHERE+name='user_000001'"
            "light_limit100  2  10   /query?sql=SELECT+*+FROM+users+LIMIT+100"
            "mid_limit1000   4  20   /query?sql=SELECT+*+FROM+users+LIMIT+1000"
            "heavy_limit5000 6  40   /query?sql=SELECT+*+FROM+users+LIMIT+5000"
        )
        ;;
    *)
        echo "[sql-bench][FAIL] invalid PROFILE=${PROFILE}, expected: S|M|L|ALL"
        exit 1
        ;;
esac

# ── 检测可用端口 ──
find_free_port() {
    local port
    for port in $(seq 18080 18100); do
        if ! ss -tln 2>/dev/null | grep -q ":${port} "; then
            echo "${port}"
            return 0
        fi
    done
    echo ""
}

PORT="$(find_free_port)"
if [ -z "${PORT}" ]; then
    echo "[sql-bench][FAIL] no free port found"
    exit 1
fi

echo "[sql-bench] profile=${PROFILE}"
echo "[sql-bench] port=${PORT}"
echo "[sql-bench] duration=${DURATION}"
echo "[sql-bench] output=${OUT_MD}"

# ── 启动服务器 ──
pkill -f 'build/main_run' >/dev/null 2>&1 || true
sleep 1
./build/main_run "${PORT}" "${ROOT_DIR}" epoll 12 close >/tmp/cppreactor_sqlbench.log 2>&1 &
SERVER_PID=$!

# ── 等待服务器端口就绪，超时 5 秒 ──
echo "[sql-bench] waiting for server on port ${PORT}..."
for i in $(seq 1 10); do
    sleep 0.5
    if ss -tln 2>/dev/null | grep -q ":${PORT} "; then
        echo "[sql-bench] server is ready (pid=${SERVER_PID})"
        break
    fi
    if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
        echo "[sql-bench][FAIL] server died during startup"
        cat /tmp/cppreactor_sqlbench.log
        exit 1
    fi
done

# 循环结束后还没监听则失败
if ! ss -tln 2>/dev/null | grep -q ":${PORT} "; then
    echo "[sql-bench][FAIL] server failed to start (timeout)"
    cat /tmp/cppreactor_sqlbench.log
    kill "${SERVER_PID}" 2>/dev/null || true
    exit 1
fi

# ── 验证 SQL 接口可正常响应 ──
echo "[sql-bench] sanity check: /healthz"
HEALTH_CODE="$(curl -s -o /dev/null -w "%{http_code}" "http://127.0.0.1:${PORT}/healthz" 2>/dev/null || true)"
if [ "${HEALTH_CODE}" != "200" ]; then
    echo "[sql-bench][FAIL] healthz failed (code=${HEALTH_CODE})"
    cat /tmp/cppreactor_sqlbench.log
    kill "${SERVER_PID}" 2>/dev/null || true
    exit 1
fi

# ── 写入 Markdown 表头 ──
cat >"${OUT_MD}" <<EOF
# SQL 压测报告

| 用例 | profile | wrk线程 | 并发数 | duration | 总请求 | QPS | 平均延迟 | 延迟分布 |
|------|---------|-------:|------:|:--------|------:|----:|--------:|---------|
EOF

# ── 逐个执行压测 ──
ANY_FAILED=false
for entry in "${QUERIES[@]}"; do
    read -r name wrk_threads conns url <<<"${entry}"

    echo "[sql-bench] running: ${name} (wrk=${wrk_threads}, conns=${conns})"
    full_url="http://127.0.0.1:${PORT}${url}"

    wrk_out="$(wrk -t"${wrk_threads}" -c"${conns}" -d"${DURATION}" --latency "${full_url}" 2>&1 || true)"

    requests="$(echo "${wrk_out}" | awk '/requests in / {print $1; exit}')"
    rps="$(echo "${wrk_out}" | awk '/Requests\/sec:/ {print $2; exit}')"
    latency="$(echo "${wrk_out}" | awk '/^[[:space:]]*Latency/ {print $2; exit}')"
    p50="$(echo "${wrk_out}" | awk '/50%/ {print $2; exit}')"
    p99="$(echo "${wrk_out}" | awk '/99%/ {print $2; exit}')"

    requests="${requests:-0}"
    rps="${rps:-0}"
    latency="${latency:--}"
    p50="${p50:--}"
    p99="${p99:--}"

    # 如果 wrk 完全没拿到请求，说明服务器可能挂了，提前退出
    if [ "${requests}" = "0" ] || [ "${requests}" = "" ]; then
        echo "[sql-bench][WARN] ${name} got 0 requests, server may be down"
        ANY_FAILED=true
    fi

    if [ "${p50}" != "-" ] && [ "${p99}" != "-" ]; then
        echo "| ${name} | ${PROFILE} | ${wrk_threads} | ${conns} | ${DURATION} | ${requests} | ${rps} | ${latency} | p50=${p50} / p99=${p99} |" >>"${OUT_MD}"
    else
        echo "| ${name} | ${PROFILE} | ${wrk_threads} | ${conns} | ${DURATION} | ${requests} | ${rps} | ${latency} | -- |" >>"${OUT_MD}"
    fi
done

# ── 清理 ──
kill "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
pkill -f 'build/main_run' >/dev/null 2>&1 || true

echo "[sql-bench] done"
echo ""
cat "${OUT_MD}"

if [ "${ANY_FAILED}" = "true" ]; then
    echo "[sql-bench][WARN] 部分用例未获得有效数据，请检查服务器日志: /tmp/cppreactor_sqlbench.log"
    exit 1
fi
