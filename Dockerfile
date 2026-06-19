# ============================================================================
# Stage 1: Build
# ============================================================================
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    make \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# 先复制第三方库（利用 Docker 缓存层）
COPY third_party/ /build/third_party/

# 再复制源码和构建配置
COPY . .

# 删除不需要的构建产物（避免冲突）
RUN rm -rf build/

RUN make -j$(nproc)

# ============================================================================
# Stage 2: Runtime
# ============================================================================
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# 复制编译产物
COPY --from=builder /build/build/main_run /app/main_run
COPY --from=builder /build/build/main_echo /app/main_echo
COPY --from=builder /build/config /app/config

# 复制 MySQL Connector/C++ 运行时 .so（含 symlink 和实际文件）
COPY --from=builder /build/third_party/mysql-cppconn/lib/ /app/lib/

# 复制 SSL 依赖（MySQL Connector 的传递依赖）
COPY --from=builder /usr/lib/x86_64-linux-gnu/libcrypto.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libssl.so* /usr/lib/x86_64-linux-gnu/

# 设置运行时库搜索路径
ENV LD_LIBRARY_PATH=/app/lib:${LD_LIBRARY_PATH}

# 复制入口脚本
COPY docker-entrypoint.sh /app/docker-entrypoint.sh

RUN chmod +x /app/docker-entrypoint.sh && \
    ldconfig 2>/dev/null || true

WORKDIR /app

# MySQL 连接环境变量（可在 docker-compose 或 run 时覆盖）
ENV MYSQL_HOST=mysql \
    MYSQL_PORT=3306 \
    MYSQL_USER=root \
    MYSQL_PASSWORD=root \
    MYSQL_DATABASE=test

EXPOSE 8080

ENTRYPOINT ["/app/docker-entrypoint.sh"]
CMD ["./main_run", "8080", "/app"]
