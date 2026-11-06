#!/bin/bash
set -e

# Generate SQLConfig.json from environment variables
cat > /app/config/SQLConfig.json << EOF
{
    "host": "${MYSQL_HOST:-mysql}",
    "port": ${MYSQL_PORT:-3306},
    "user": "${MYSQL_USER:-root}",
    "password": "${MYSQL_PASSWORD:-root}",
    "database": "${MYSQL_DATABASE:-test}",
    "charset": "utf8mb4",
    "connectTimeoutMs": 5000,
    "readTimeoutMs": 5000,
    "writeTimeoutMs": 5000,
    "pingBeforeUse": true,
    "maxConnLifetimeMs": 1800000,
    "maxConnIdleMs": 60000,
    "readOnly": true
}
EOF

exec "$@"
