#!/bin/bash
set -e

# Generate SQLConfig.json from environment variables
cat > /app/config/SQLConfig.json << EOF
{
    "driver": "mysql_connector_cpp",
    "host": "${MYSQL_HOST:-mysql}",
    "port": ${MYSQL_PORT:-3306},
    "user": "${MYSQL_USER:-root}",
    "password": "${MYSQL_PASSWORD:-root}",
    "database": "${MYSQL_DATABASE:-test}",
    "charset": "utf8mb4",
    "connectTimeoutMs": 5000,
    "readTimeoutMs": 5000,
    "writeTimeoutMs": 5000,
    "connMode": "thread_local_single_conn",
    "pingBeforeUse": true,
    "reconnectMaxAttempts": 3,
    "reconnectBackoffMs": 1000,
    "maxConnLifetimeMs": 1800000,
    "maxConnIdleMs": 60000,
    "initSqls": [
        "SELECT 1"
    ],
    "slowQueryMs": 1000,
    "enableSqlLog": true,
    "readOnly": true
}
EOF

exec "$@"
