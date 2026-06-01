#!/usr/bin/env python3
"""
数据注入脚本：向 MySQL 插入 50,000 条测试数据。

用法：
    bash tests/setup.sh                                    # 首次：创建虚拟环境
    source tests/.venv/bin/activate                        # 激活虚拟环境
    python3 tests/seed_data.py                             # 运行
    python3 tests/seed_data.py --host 127.0.0.1 --port 3306 --user root --password xxx --database test

表结构：
    CREATE TABLE users (
        id INT AUTO_INCREMENT PRIMARY KEY,
        name VARCHAR(64) NOT NULL,
        email VARCHAR(128) NOT NULL,
        created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
    );
"""

import argparse
import json
import os
import sys
import time
from pathlib import Path

try:
    import pymysql
except ImportError:
    print("请先创建并激活虚拟环境:")
    print("  bash tests/setup.sh")
    print("  source tests/.venv/bin/activate")
    sys.exit(1)


def load_config():
    """从 config/SQLConfig.json 加载数据库连接参数"""
    config_path = Path(__file__).resolve().parent.parent / "config" / "SQLConfig.json"
    if not config_path.exists():
        return None
    with open(config_path, "r") as f:
        cfg = json.load(f)
    return {
        "host": cfg.get("host", "127.0.0.1"),
        "port": cfg.get("port", 3306),
        "user": cfg.get("user", "root"),
        "password": cfg.get("password", ""),
        "database": cfg.get("database", "test"),
    }


def parse_args():
    parser = argparse.ArgumentParser(description="向 MySQL 插入 50,000 条测试数据")
    parser.add_argument("--host", default=None, help="MySQL 主机地址")
    parser.add_argument("--port", type=int, default=None, help="MySQL 端口")
    parser.add_argument("--user", default=None, help="MySQL 用户名")
    parser.add_argument("--password", default=None, help="MySQL 密码")
    parser.add_argument("--database", default=None, help="数据库名")
    parser.add_argument("--rows", type=int, default=50000, help="插入行数 (默认 50000)")
    parser.add_argument("--batch", type=int, default=1000, help="每批插入行数 (默认 1000)")
    return parser.parse_args()


def merge_config(args):
    """命令行参数优先，未指定则从 JSON 文件读取"""
    cfg = load_config() or {}
    for key in ("host", "port", "user", "password", "database"):
        val = getattr(args, key, None)
        if val is not None:
            cfg[key] = val
    return cfg


def create_table(cursor):
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS users (
            id INT AUTO_INCREMENT PRIMARY KEY,
            name VARCHAR(64) NOT NULL,
            email VARCHAR(128) NOT NULL,
            created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    """)


def generate_batch(start_id, count):
    rows = []
    for i in range(count):
        uid = start_id + i
        name = f"user_{uid:06d}"
        email = f"user{uid:06d}@example.com"
        rows.append((name, email))
    return rows


def seed_data(conn, total_rows, batch_size):
    cursor = conn.cursor()

    cursor.execute("SELECT COUNT(*) FROM users")
    existing = cursor.fetchone()[0]
    if existing > 0:
        print(f"[info] users 表已有 {existing} 行数据，将追加插入")
    else:
        print("[info] users 表为空，开始插入")

    sql = "INSERT INTO users (name, email) VALUES (%s, %s)"
    inserted = 0
    batches = (total_rows + batch_size - 1) // batch_size
    start_time = time.time()

    for batch_idx in range(batches):
        remaining = total_rows - inserted
        cur_batch = min(batch_size, remaining)
        rows = generate_batch(inserted + 1, cur_batch)

        cursor.executemany(sql, rows)
        conn.commit()
        inserted += cur_batch

        elapsed = time.time() - start_time
        rate = inserted / elapsed if elapsed > 0 else 0
        print(
            f"\r[progress] {inserted}/{total_rows} 行 "
            f"({inserted * 100 // total_rows}%)  "
            f"{rate:.0f} rows/s  "
            f"elapsed={elapsed:.1f}s",
            end="",
            flush=True,
        )

    print()
    return inserted


def main():
    args = parse_args()
    cfg = merge_config(args)

    required = ("host", "port", "user", "database")
    for key in required:
        if not cfg.get(key):
            print(f"[error] 缺少必要参数: {key}")
            sys.exit(1)

    print(f"[config] 连接 MySQL: {cfg['user']}@{cfg['host']}:{cfg['port']}/{cfg['database']}")

    conn = pymysql.connect(
        host=cfg["host"],
        port=cfg["port"],
        user=cfg["user"],
        password=cfg.get("password", ""),
        database=cfg["database"],
        charset="utf8mb4",
    )

    try:
        with conn.cursor() as cursor:
            create_table(cursor)
        conn.commit()

        total = seed_data(conn, args.rows, args.batch)

        with conn.cursor() as cursor:
            cursor.execute("SELECT COUNT(*) FROM users")
            final_count = cursor.fetchone()[0]

        print(f"[done] 插入完成！users 表现有 {final_count} 行数据")

    finally:
        conn.close()


if __name__ == "__main__":
    main()
