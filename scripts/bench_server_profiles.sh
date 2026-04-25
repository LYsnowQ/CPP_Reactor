#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${1:-/project/CPPReactor}"
PROFILE="${2:-M}"
OUT_MD="${3:-/project/CPPReactor/tests/benchmarks/bench_profile_${PROFILE}_latest.md}"
DURATION="${4:-12s}"

if ! command -v wrk >/dev/null 2>&1; then
    echo "[profile][FAIL] wrk not found"
    exit 1
fi

cd "${ROOT_DIR}"
mkdir -p "$(dirname "${OUT_MD}")"

cpu_threads="$(nproc)"

case "${PROFILE}" in
    S)
        # 4C8G 近似档位
        CASES=(
            "S1 epoll 2 keepalive 2 80 18301"
            "S2 epoll 3 keepalive 3 120 18302"
            "S3 epoll 3 close     3 120 18303"
        )
        ;;
    M)
        # 8C16G 近似档位
        CASES=(
            "M1 epoll 4 keepalive 4 150 18311"
            "M2 epoll 6 keepalive 6 240 18312"
            "M3 epoll 6 close     6 240 18313"
            "M4 poll  6 keepalive 6 240 18314"
        )
        ;;
    L)
        # 16C32G 近似档位
        CASES=(
            "L1 epoll 8  keepalive 8  300 18321"
            "L2 epoll 12 keepalive 12 500 18322"
            "L3 epoll 12 close     12 500 18323"
            "L4 poll  12 keepalive 12 500 18324"
        )
        ;;
    LOCAL_MAX)
        # 本机上限探索档位，按当前机器线程数自适配
        io_mid=$((cpu_threads / 2))
        if [ "${io_mid}" -lt 4 ]; then io_mid=4; fi
        io_high=$((cpu_threads * 3 / 4))
        if [ "${io_high}" -lt "${io_mid}" ]; then io_high="${io_mid}"; fi
        wrk_mid=$((cpu_threads / 2))
        if [ "${wrk_mid}" -lt 4 ]; then wrk_mid=4; fi
        wrk_high="${cpu_threads}"
        if [ "${wrk_high}" -gt 16 ]; then wrk_high=16; fi
        CASES=(
            "X1 epoll ${io_mid} keepalive ${wrk_mid} 300 18331"
            "X2 epoll ${io_high} keepalive ${wrk_high} 500 18332"
            "X3 epoll ${io_high} close     ${wrk_high} 500 18333"
        )
        ;;
    *)
        echo "[profile][FAIL] invalid PROFILE=${PROFILE}, expected: S|M|L|LOCAL_MAX"
        exit 1
        ;;
esac

echo "[profile] root=${ROOT_DIR}"
echo "[profile] profile=${PROFILE}"
echo "[profile] output=${OUT_MD}"
echo "[profile] duration=${DURATION}"

cat >"${OUT_MD}" <<EOF
| case | profile | dispatcher | io_threads | conn_mode | wrk_threads | connections | duration | requests | req_per_sec | latency_avg | transfer_sec | socket_errors |
|---|---|---|---:|---|---:|---:|---|---:|---:|---|---|---|
EOF

run_case() {
    local case_name="$1"
    local dispatcher="$2"
    local io_threads="$3"
    local conn_mode="$4"
    local wrk_threads="$5"
    local conns="$6"
    local port="$7"

    pkill -f './main_run' >/dev/null 2>&1 || true
    ./main_run "${port}" "${ROOT_DIR}" "${dispatcher}" "${io_threads}" "${conn_mode}" 100 10000 >/tmp/cppreactor_profile_${case_name}.log 2>&1 &
    local server_pid=$!
    sleep 1

    local wrk_out
    if ! kill -0 "${server_pid}" >/dev/null 2>&1; then
        wrk_out="[start_failed] server failed to start"
    else
        wrk_out="$(wrk -t"${wrk_threads}" -c"${conns}" -d"${DURATION}" --latency "http://127.0.0.1:${port}/" 2>&1 || true)"
    fi

    local requests
    requests="$(echo "${wrk_out}" | awk '/requests in / {print $1; exit}')"
    local rps
    rps="$(echo "${wrk_out}" | awk '/Requests\/sec:/ {print $2; exit}')"
    local latency
    latency="$(echo "${wrk_out}" | awk '/^[[:space:]]*Latency/ {print $2; exit}')"
    local transfer
    transfer="$(echo "${wrk_out}" | awk '/Transfer\/sec:/ {print $2; exit}')"
    local sock_err
    sock_err="$(echo "${wrk_out}" | awk -F': ' '/Socket errors:/ {print $2; exit}')"

    requests="${requests:-0}"
    rps="${rps:-0}"
    latency="${latency:--}"
    transfer="${transfer:--}"
    sock_err="${sock_err:--}"

    echo "| ${case_name} | ${PROFILE} | ${dispatcher} | ${io_threads} | ${conn_mode} | ${wrk_threads} | ${conns} | ${DURATION} | ${requests} | ${rps} | ${latency} | ${transfer} | ${sock_err} |" >>"${OUT_MD}"

    kill "${server_pid}" >/dev/null 2>&1 || true
    wait "${server_pid}" 2>/dev/null || true
}

for one in "${CASES[@]}"; do
    # shellcheck disable=SC2086
    run_case ${one}
done

pkill -f './main_run' >/dev/null 2>&1 || true
echo "[profile] done"
cat "${OUT_MD}"
