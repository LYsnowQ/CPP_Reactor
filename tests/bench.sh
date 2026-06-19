#!/usr/bin/env bash
set -euo pipefail

# ====================================================================
# 统一压测脚本
#
# 用法:
#   bash tests/bench.sh static M              # HTTP 多配置探索
#   bash tests/bench.sh static GRADIENT 8     # HTTP 梯度加压，每档 8 秒
#   bash tests/bench.sh echo ECHO             # Echo 梯度加压
#   bash tests/bench.sh echo ECHO_THREADS     # Echo 线程梯度
#
# 必须在项目根目录下运行: bash tests/bench.sh <mode> <subprofile> [duration_sec]
# ====================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${ROOT_DIR}"

PROFILE="${1:-static}"
SUBPROFILE="${2:-GRADIENT}"
DURATION="${3:-12}"
PORT_HTTP=8080
PORT_ECHO=8081

BIN_HTTP="${ROOT_DIR}/main_run"
BIN_ECHO="${ROOT_DIR}/main_echo"
OUT_DIR="${ROOT_DIR}/tests/benchmarks"
SERVER_LOG="/tmp/cppreactor_bench_server.log"
PID_FILE="/tmp/cppreactor_bench_server.pid"

WRK_BIN="wrk"

if ! command -v "${WRK_BIN}" &>/dev/null; then
    echo "[bench][FAIL] wrk not found"
    exit 1
fi

mkdir -p "${OUT_DIR}"

# 检查二进制
if [[ ! -x "${BIN_HTTP}" && ! -x "${BIN_ECHO}" ]]; then
    echo "[bench][FAIL] neither ${BIN_HTTP} nor ${BIN_ECHO} found. Run 'make' first."
    exit 1
fi

cpu_threads="$(nproc)"
DURATION_S="${DURATION}s"

# ══════════════════════════════════════════════════════════════════
# 服务器生命周期（通用）
# ══════════════════════════════════════════════════════════════════

stop_server() {
    if [[ -f "${PID_FILE}" ]]; then
        local pid
        pid="$(cat "${PID_FILE}")"
        kill "${pid}" 2>/dev/null || true
        wait "${pid}" 2>/dev/null || true
        rm -f "${PID_FILE}"
    fi
    pkill -f "${BIN_HTTP}" 2>/dev/null || true
    pkill -f "${BIN_ECHO}" 2>/dev/null || true
}

trap stop_server EXIT

# ── 启动 HTTP 服务器 (main_run) ──
start_http() {
    local dispatcher="$1" threads="$2" conn_mode="$3"
    local ka_max="${4:-100}" ka_idle="${5:-10000}"
    stop_server
    "${BIN_HTTP}" "${PORT_HTTP}" "${ROOT_DIR}" "${dispatcher}" "${threads}" \
        "${conn_mode}" "${ka_max}" "${ka_idle}" > "${SERVER_LOG}" 2>&1 &
    local pid=$!
    echo "${pid}" > "${PID_FILE}"
    for i in $(seq 1 10); do
        sleep 0.5
        if ss -tln 2>/dev/null | grep -q ":${PORT_HTTP} "; then
            echo "[bench] HTTP server started (pid=${pid}, ${dispatcher}/${threads}th/${conn_mode})"
            return 0
        fi
        if ! kill -0 "${pid}" 2>/dev/null; then
            echo "[bench][FAIL] HTTP server died during startup"
            cat "${SERVER_LOG}"; return 1
        fi
    done
    echo "[bench][FAIL] HTTP server timeout"
    return 1
}

# ── 启动 Echo 服务器 (main_echo) ──
start_echo() {
    local threads="$1"
    stop_server
    "${BIN_ECHO}" "${PORT_ECHO}" "${threads}" > "${SERVER_LOG}" 2>&1 &
    local pid=$!
    echo "${pid}" > "${PID_FILE}"
    for i in $(seq 1 10); do
        sleep 0.5
        if ss -tln 2>/dev/null | grep -q ":${PORT_ECHO} "; then
            echo "[bench] Echo server started (pid=${pid}, ${threads}th)"
            return 0
        fi
        if ! kill -0 "${pid}" 2>/dev/null; then
            echo "[bench][FAIL] Echo server died during startup"
            cat "${SERVER_LOG}"; return 1
        fi
    done
    echo "[bench][FAIL] Echo server timeout"
    return 1
}

# ══════════════════════════════════════════════════════════════════
# wrk 单次压测
# ══════════════════════════════════════════════════════════════════

run_wrk() {
    local case_name="$1" wrk_t="$2" wrk_c="$3" port="$4" latency_flag="${5:+--latency}"

    local wrk_out
    wrk_out="$("${WRK_BIN}" -t"${wrk_t}" -c"${wrk_c}" -d"${DURATION_S}" \
        ${latency_flag} "http://127.0.0.1:${port}/" 2>&1 || true)"

    local requests rps latency transfer sock_err
    requests="$(echo "${wrk_out}" | awk '/requests in / {print $1; exit}')"
    rps="$(echo "${wrk_out}" | awk '/Requests\/sec:/ {print $2; exit}')"
    latency="$(echo "${wrk_out}" | awk '/^[[:space:]]*Latency/ {print $2; exit}')"
    transfer="$(echo "${wrk_out}" | awk '/Transfer\/sec:/ {print $2; exit}')"
    sock_err="$(echo "${wrk_out}" | awk -F': ' '/Socket errors:/ {print $2; exit}')"

    requests="${requests:-0}"; rps="${rps:-0}"
    latency="${latency:--}"; transfer="${transfer:--}"; sock_err="${sock_err:--}"

    echo "| ${case_name} | ${wrk_t} | ${wrk_c} | ${DURATION_S} | ${requests} | ${rps} | ${latency} | ${transfer} | ${sock_err} |" >> "${OUT_MD}"
    echo "${rps}"
}

# ── 触顶检测（返回 0/1） ──
check_plateau() {
    local rps="$1" prev="$2" name="$3" pcount="$4"
    if [[ $(echo "${prev} > 0" | bc -l 2>/dev/null) != 1 ]]; then
        echo 0; return
    fi
    local pct
    pct="$(echo "scale=2; (${rps} - ${prev}) / ${prev} * 100" | bc -l 2>/dev/null || echo 0)"
    local is_slow is_neg
    is_slow="$(echo "${pct} < 5" | bc -l 2>/dev/null || echo 0)"
    is_neg="$(echo "${pct} < -5" | bc -l 2>/dev/null || echo 0)"

    if [[ ${is_neg} == 1 ]]; then
        local new=$((pcount + 1))
        echo "[bench] ${name}: QPS 下降 ${pct}% (${new}/2)" >&2
        echo "${new}"
    elif [[ ${is_slow} == 1 ]]; then
        local new=$((pcount + 1))
        echo "[bench] ${name}: 增长趋缓 ${pct}% (${new}/2)" >&2
        echo "${new}"
    else
        echo 0
    fi
}

# ══════════════════════════════════════════════════════════════════
# 表头
# ══════════════════════════════════════════════════════════════════

init_md() {
    local title="$1"
    OUT_MD="${OUT_DIR}/bench_$(date +%Y%m%d_%H%M%S).md"
    cat > "${OUT_MD}" <<EOF
# ${title}

**日期**: $(date '+%Y-%m-%d %H:%M:%S')
**CPU 核数**: ${cpu_threads}
**wrk**: $("${WRK_BIN}" --version 2>/dev/null | head -1)

| case | wrk_threads | connections | duration | requests | req_per_sec | latency_avg | transfer_sec | socket_errors |
|-----|-----------:|----------:|:--------|--------:|----------:|-----------|------------|-------------|
EOF
}

# ══════════════════════════════════════════════════════════════════
# Profile: HTTP GRADIENT — 固定 epoll 12 keepalive 梯度增加并发
# ══════════════════════════════════════════════════════════════════

profile_HTTP_GRADIENT() {
    init_md "HTTP 静态文件压测 — 梯度加压"
    start_http "epoll" 12 "keepalive" || return 1

    local -a CONNS=(50 100 200 500 800 1000 1500 2000)
    local -a WTHRS=(4 4 8 12 12 16 16 20)
    local prev=0 peak=0 pcase="" pc=0
    local conn wt name rps

    for i in "${!CONNS[@]}"; do
        conn="${CONNS[$i]}" wt="${WTHRS[$i]}" name="H_c${conn}"
        rps="$(run_wrk "${name}" "${wt}" "${conn}" "${PORT_HTTP}" "latency")"
        [[ $(echo "${rps} > ${peak}" | bc -l 2>/dev/null) == 1 ]] && peak="${rps}" && pcase="${name}"
        pc="$(check_plateau "${rps}" "${prev}" "${name}" "${pc}")"
        [[ ${pc} -ge 2 ]] && echo "[bench] 已达上限，停止" && break
        prev="${rps}"
    done
    stop_server
    echo -e "\n**峰值 QPS**: ${peak} (${pcase})" >> "${OUT_MD}"
    echo -e "**服务器**: epoll 12 threads keepalive\n" >> "${OUT_MD}"
}

# ══════════════════════════════════════════════════════════════════
# Profile: HTTP M — 多配置探索
# ══════════════════════════════════════════════════════════════════

profile_HTTP_M() {
    init_md "HTTP 静态文件压测 — 多配置探索"
    local cases=(
        "M1:epoll:8:keepalive:8:300"
        "M2:epoll:12:keepalive:12:500"
        "M3:epoll:16:keepalive:16:800"
        "M4:epoll:12:close:12:500"
        "M5:poll:12:keepalive:12:500"
    )
    for entry in "${cases[@]}"; do
        IFS=':' read -r name disp threads mode wt conns <<< "${entry}"
        start_http "${disp}" "${threads}" "${mode}" || continue
        run_wrk "${name}" "${wt}" "${conns}" "${PORT_HTTP}" "" > /dev/null
        stop_server
    done
}

# ══════════════════════════════════════════════════════════════════
# Profile: ECHO — 固定 12 服务器线程，梯度增加并发
# ══════════════════════════════════════════════════════════════════

profile_ECHO() {
    init_md "Echo 基准压测 — 梯度加压"
    start_echo 12 || return 1

    local -a CONNS=(10 50 100 200 500 800 1000 1500)
    local -a WTHRS=(2 4 4 8 12 12 16 16)
    local prev=0 peak=0 pcase="" pc=0
    local conn wt name rps

    for i in "${!CONNS[@]}"; do
        conn="${CONNS[$i]}" wt="${WTHRS[$i]}" name="E_c${conn}"
        rps="$(run_wrk "${name}" "${wt}" "${conn}" "${PORT_ECHO}" "latency")"
        [[ $(echo "${rps} > ${peak}" | bc -l 2>/dev/null) == 1 ]] && peak="${rps}" && pcase="${name}"
        pc="$(check_plateau "${rps}" "${prev}" "${name}" "${pc}")"
        [[ ${pc} -ge 2 ]] && echo "[bench] 已达上限，停止" && break
        prev="${rps}"
    done
    stop_server
    echo -e "\n**峰值 QPS**: ${peak} (${pcase})" >> "${OUT_MD}"
    echo -e "**服务器**: epoll 12 threads\n" >> "${OUT_MD}"
}

# ══════════════════════════════════════════════════════════════════
# Profile: ECHO_THREADS — 固定 500 并发，逐档增加服务器线程
# ══════════════════════════════════════════════════════════════════

profile_ECHO_THREADS() {
    init_md "Echo 基准压测 — 线程梯度"
    local prev=0 peak=0 pcase="" pc=0
    local rps

    for io in $(seq 2 2 32); do
        start_echo "${io}" || break
        rps="$(run_wrk "ET_io${io}" 12 500 "${PORT_ECHO}" "")"
        stop_server
        [[ $(echo "${rps} > ${peak}" | bc -l 2>/dev/null) == 1 ]] && peak="${rps}" && pcase="ET_io${io}"
        pc="$(check_plateau "${rps}" "${prev}" "ET_io${io}" "${pc}")"
        [[ ${pc} -ge 2 ]] && echo "[bench] 已达上限，停止" && break
        prev="${rps}"
    done
    echo -e "\n**峰值 QPS**: ${peak} (${pcase})" >> "${OUT_MD}"
    echo -e "**wrk**: 12 线程 / 500 并发\n" >> "${OUT_MD}"
}

# ══════════════════════════════════════════════════════════════════
# 分发
# ══════════════════════════════════════════════════════════════════

case "${PROFILE}" in
    static|STATIC)
        case "${SUBPROFILE}" in
            M|GRADIENT) "profile_HTTP_${SUBPROFILE}" ;;
            *) echo "[bench][FAIL] unknown static profile: ${SUBPROFILE}"; exit 1 ;;
        esac
        ;;
    echo|ECHO)
        case "${SUBPROFILE}" in
            ECHO|ECHO_THREADS) "profile_${SUBPROFILE}" ;;
            *) echo "[bench][FAIL] unknown echo profile: ${SUBPROFILE}"; exit 1 ;;
        esac
        ;;
    *)
        echo "[bench][FAIL] unknown mode: ${PROFILE}"
        echo "  Usage: bash tests/bench.sh <static|echo> <profile> [duration_sec]"
        echo "  static profiles: M, GRADIENT"
        echo "  echo profiles:   ECHO, ECHO_THREADS"
        exit 1
        ;;
esac

echo ""
echo "[bench] done → ${OUT_MD}"
cat "${OUT_MD}"
