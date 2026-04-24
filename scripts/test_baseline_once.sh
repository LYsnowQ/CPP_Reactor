#!/usr/bin/env bash
set -euo pipefail

URL="${1:-http://127.0.0.1:8080/}"
DURATION="${2:-20s}"
CONNECTIONS="${3:-200}"
THREADS="${4:-4}"

echo "[baseline] running one-shot baseline"
echo "[baseline] url=${URL} duration=${DURATION} connections=${CONNECTIONS} threads=${THREADS}"

"$(dirname "$0")/bench_http.sh" "${URL}" "${DURATION}" "${CONNECTIONS}" "${THREADS}"

echo "[baseline] done. fill docs/PERF_BASELINE_TEMPLATE.md with this run result."

