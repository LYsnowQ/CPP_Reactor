#!/usr/bin/env python3
import socket
import sys
import time
from typing import Tuple, List


def send_raw(host: str, port: int, payload: bytes, timeout: float = 3.0) -> bytes:
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        sock.sendall(payload)
        chunks: List[bytes] = []
        while True:
            try:
                data = sock.recv(4096)
            except socket.timeout:
                break
            if not data:
                break
            chunks.append(data)
    return b"".join(chunks)


def send_split(host: str, port: int, p1: bytes, p2: bytes, gap_sec: float = 0.2, timeout: float = 3.0) -> bytes:
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        sock.sendall(p1)
        time.sleep(gap_sec)
        sock.sendall(p2)
        chunks: List[bytes] = []
        while True:
            try:
                data = sock.recv(4096)
            except socket.timeout:
                break
            if not data:
                break
            chunks.append(data)
    return b"".join(chunks)


def status_line(resp: bytes) -> str:
    if not resp:
        return ""
    first_line = resp.split(b"\r\n", 1)[0]
    return first_line.decode("latin1", errors="replace")


def contains_status(resp: bytes, code: int) -> bool:
    return f" {code} ".encode() in resp[:64]


def run(host: str, port: int) -> int:
    # 保持请求体大于 1MB，用于触发 413（服务端上限当前为 1MB）
    huge_body = b"a" * (1024 * 1024 + 128)

    cases = [
        {
            "name": "valid_get",
            "run": lambda: send_raw(
                host,
                port,
                b"GET / HTTP/1.1\r\nHost: local\r\n\r\n",
            ),
            "expect": 200,
        },
        {
            "name": "invalid_request_line",
            "run": lambda: send_raw(
                host,
                port,
                b"BADLINE\r\nHost: local\r\n\r\n",
            ),
            "expect": 400,
        },
        {
            "name": "payload_too_large",
            "run": lambda: send_raw(
                host,
                port,
                b"POST /upload HTTP/1.1\r\nHost: local\r\nContent-Length: "
                + str(len(huge_body)).encode()
                + b"\r\n\r\n"
                + huge_body,
                timeout=5.0,
            ),
            "expect": 413,
        },
        {
            "name": "split_content_length_body",
            "run": lambda: send_split(
                host,
                port,
                b"POST /x HTTP/1.1\r\nHost: local\r\nContent-Length: 10\r\n\r\n12345",
                b"67890",
            ),
            "expect": 200,
        },
    ]

    failed = 0
    for case in cases:
        try:
            resp = case["run"]()
            ok = contains_status(resp, case["expect"])
            line = status_line(resp)
            if ok:
                print(f"[PASS] {case['name']} -> {line}")
            else:
                failed += 1
                print(f"[FAIL] {case['name']} expected={case['expect']} got={line or '<empty>'}")
        except Exception as exc:
            failed += 1
            print(f"[FAIL] {case['name']} exception={exc}")

    total = len(cases)
    print(f"\nSummary: total={total} failed={failed} passed={total - failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    h = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    p = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
    sys.exit(run(h, p))
