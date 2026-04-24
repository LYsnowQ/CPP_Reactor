#!/usr/bin/env bash
set -euo pipefail

URL="${1:-http://127.0.0.1:8080/}"

echo "[smoke] target=${URL}"

CODE="$(curl -s -o /tmp/cppreactor_smoke_body.$$ -w "%{http_code}" "${URL}" || true)"
if [[ "${CODE}" == "200" || "${CODE}" == "400" || "${CODE}" == "408" || "${CODE}" == "413" ]]; then
    echo "[smoke][PASS] http_code=${CODE}"
    rm -f /tmp/cppreactor_smoke_body.$$
    exit 0
fi

echo "[smoke][FAIL] unexpected http_code=${CODE}"
echo "[smoke] response_body:"
cat /tmp/cppreactor_smoke_body.$$ || true
rm -f /tmp/cppreactor_smoke_body.$$
exit 1

