# ============================================================================
# Stage 1: Build
# ============================================================================
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    make \
    libmysqlcppconn-dev \
    nlohmann-json3-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY . .

RUN make -j$(nproc)

# ============================================================================
# Stage 2: Runtime
# ============================================================================
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Copy the compiled binary
COPY --from=builder /build/main_run /app/main_run
COPY --from=builder /build/config /app/config

# Copy MySQL connector runtime libs (symlinks + actual .so)
COPY --from=builder /lib/x86_64-linux-gnu/libmysqlcppconn.so* /lib/x86_64-linux-gnu/
COPY --from=builder /lib/x86_64-linux-gnu/libmysqlcppconn.so.10 /lib/x86_64-linux-gnu/
COPY --from=builder /lib/x86_64-linux-gnu/libmysqlcppconn.so.10.9.7.0 /lib/x86_64-linux-gnu/

# Copy SSL libs (MySQL connector dependency)
COPY --from=builder /usr/lib/x86_64-linux-gnu/libcrypto.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libssl.so* /usr/lib/x86_64-linux-gnu/

# Copy entrypoint
COPY docker-entrypoint.sh /app/docker-entrypoint.sh

RUN chmod +x /app/docker-entrypoint.sh && \
    ldconfig 2>/dev/null || true

WORKDIR /app

# MySQL connection defaults (overridable via environment)
ENV MYSQL_HOST=mysql \
    MYSQL_PORT=3306 \
    MYSQL_USER=root \
    MYSQL_PASSWORD=root \
    MYSQL_DATABASE=test

EXPOSE 8080

ENTRYPOINT ["/app/docker-entrypoint.sh"]
CMD ["./main_run", "8080", "/app"]
