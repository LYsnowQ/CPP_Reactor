#!/usr/bin/env bash
# SQL 端点安全自检：默认关闭、只读护栏、错误参数。
# 需要已构建的 build/main_run；不需要真实数据库。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT_DIR}/build/main_run"
PORT=18096
TMP_DIR="$(mktemp -d)"
SERVER_PID=""

cleanup() {
    if [[ -n "${SERVER_PID}" ]]; then
        kill "${SERVER_PID}" 2>/dev/null || true
    fi
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

if [[ ! -x "${BIN}" ]]; then
    echo "[sql-security][FAIL] ${BIN} 不存在，请先 make"
    exit 1
fi

mkdir -p "${TMP_DIR}/config"
cat > "${TMP_DIR}/config/SQLConfig.json" <<'JSON'
{
    "driver": "mysql_connector_cpp",
    "host": "127.0.0.1",
    "port": 1,
    "user": "root",
    "password": "",
    "database": "test",
    "charset": "utf8mb4",
    "connectTimeoutMs": 1000,
    "readTimeoutMs": 1000,
    "writeTimeoutMs": 1000,
    "connMode": "thread_local_single_conn",
    "pingBeforeUse": true,
    "reconnectMaxAttempts": 1,
    "reconnectBackoffMs": 100,
    "maxConnLifetimeMs": 600000,
    "maxConnIdleMs": 60000,
    "initSqls": ["SELECT 1"],
    "slowQueryMs": 1000,
    "enableSqlLog": false,
    "readOnly": true
}
JSON

"${BIN}" "${PORT}" "${TMP_DIR}" epoll 2 close --enable-sql > "${TMP_DIR}/server.log" 2>&1 &
SERVER_PID=$!

ready=0
for _ in $(seq 1 40); do
    if curl -fsS "http://127.0.0.1:${PORT}/healthz" > /dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 0.25
done

if [[ "${ready}" != "1" ]]; then
    echo "[sql-security][FAIL] 服务未就绪"
    cat "${TMP_DIR}/server.log"
    exit 1
fi

failed=0

check_sql() {
    local name="$1" sql="$2" expected="$3"
    local code
    code="$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 \
        --get --data-urlencode "sql=${sql}" \
        "http://127.0.0.1:${PORT}/sql" || true)"
    if [[ "${code}" == "${expected}" ]]; then
        echo "[PASS] ${name} -> ${code}"
    else
        echo "[FAIL] ${name} expected=${expected} got=${code:-<none>}"
        failed=$((failed + 1))
    fi
}

check_sql "write_drop"        "DROP TABLE users" 403
check_sql "write_update"      "update users set name='x'" 403
check_sql "multi_statement"   "SELECT 1; DROP TABLE users" 403
check_sql "comment_prefix"    "/* x */ DROP TABLE users" 403
# 数据库不可达：必须回 500，不能挂起；连续 3 次验证工作线程未卡死。
check_sql "select_db_down_1"  "SELECT 1" 500
check_sql "select_db_down_2"  "SELECT 1" 500
check_sql "select_db_down_3"  "SELECT 1" 500

code="$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "http://127.0.0.1:${PORT}/sql" || true)"
if [[ "${code}" == "400" ]]; then
    echo "[PASS] missing_sql -> 400"
else
    echo "[FAIL] missing_sql expected=400 got=${code:-<none>}"
    failed=$((failed + 1))
fi

echo ""
if [[ "${failed}" -ne 0 ]]; then
    echo "[sql-security] ${failed} 个用例失败"
    cat "${TMP_DIR}/server.log"
    exit 1
fi

echo "[sql-security] 全部通过（未验证真实数据库查询）"
