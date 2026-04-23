#!/usr/bin/env bash
set -euo pipefail

TARGET_URL="${1:-http://127.0.0.1:8080/}"
DURATION="${2:-15s}"
CONNECTIONS="${3:-100}"
THREADS="${4:-4}"

echo "[bench] target=${TARGET_URL} duration=${DURATION} connections=${CONNECTIONS} threads=${THREADS}"

if command -v wrk >/dev/null 2>&1; then
    echo "[bench] using wrk"
    wrk -t"${THREADS}" -c"${CONNECTIONS}" -d"${DURATION}" --latency "${TARGET_URL}"
    exit 0
fi

if command -v ab >/dev/null 2>&1; then
    echo "[bench] using ab (ApacheBench)"
    SEC="${DURATION%s}"
    : "${SEC:=15}"
    # ab is request-count based, keep a simple conversion: req ~= connections * seconds * 20
    REQ=$((CONNECTIONS * SEC * 20))
    if [ "${REQ}" -lt 1000 ]; then
        REQ=1000
    fi
    ab -n "${REQ}" -c "${CONNECTIONS}" "${TARGET_URL}"
    exit 0
fi

echo "[bench] no wrk/ab found, fallback to curl loop (not a true pressure test)"
END_TIME=$((SECONDS + 10))
COUNT=0
while [ "${SECONDS}" -lt "${END_TIME}" ]; do
    curl -s -o /dev/null "${TARGET_URL}" || true
    COUNT=$((COUNT + 1))
done
echo "[bench] curl_loop_requests=${COUNT} in 10s"

