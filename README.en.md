<div align="center">

# CPPReactor

A C++20 Reactor-pattern HTTP server (learning project)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-PolyForm%20Noncommercial%201.0.0-orange.svg)](./LICENSING.en.md)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20WSL2-lightgrey.svg)](#requirements)
[![CI](https://github.com/LYsnowQ/CPP_Reactor/actions/workflows/ci.yml/badge.svg)](https://github.com/LYsnowQ/CPP_Reactor/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-2.0.0-informational.svg)](./CHANGELOG.md)

</div>

[简体中文](README.md) | English

> **Positioning**
>
> This is a personal C++ networking learning project: it explores a multi-threaded Reactor,
> HTTP/1.1 parsing, connection lifetime management, and asynchronous MySQL queries.
> **It is not a production server.** There is no TLS and no authentication.
> `/sql` is disabled by default; when enabled, it only allows read-only statements by default,
> but the service is still unsuitable for the public internet.
> See [Security boundaries](#security-boundaries) and
> [GitHub Issues](https://github.com/LYsnowQ/CPP_Reactor/issues).

## Table of contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Requirements](#requirements)
- [Quick start](#quick-start)
- [Configuration](#configuration)
- [HTTP API](#http-api)
- [Tests and benchmarks](#tests-and-benchmarks)
- [Docker](#docker)
- [Project layout](#project-layout)
- [Performance baseline](#performance-baseline)
- [Documentation](#documentation)
- [Version and license](#version-and-license)

---

## Overview

CPPReactor implements an HTTP/1.1 server from scratch in C++20. A main thread accepts
connections; multiple I/O threads each run an `EventLoop`; the event backend can be switched
between **epoll / poll / select** at runtime. On top of that it provides static file serving,
directory indexes, `/healthz`, and an asynchronous MySQL query demo endpoint built on
MySQL Connector/C++.

The current version is **2.0.0** (2026-11-06). See [CHANGELOG.md](./CHANGELOG.md) for the
version history and [docs/iterations/](./docs/iterations/README.md) for the 29 historical
iteration notes.

### Scope

| Good for | Not intended for |
|----------|------------------|
| Learning Reactor, epoll, and incremental HTTP parsing | Production or public-internet deployment |
| A reference implementation for C++ network programming | TLS, authentication, rate limiting |
| Benchmark methodology and performance experiments | Large-file transfer or streaming responses (files are read into memory) |

---

## Features

| Feature | Description | Current limitation |
|---------|-------------|--------------------|
| **Multi-threaded Reactor** | Main-thread accept, N I/O event loops, cross-thread `post()` callbacks | No graceful shutdown / signal handling |
| **Pluggable dispatchers** | epoll / poll / select switchable at runtime | epoll is the default |
| **HTTP/1.1 parsing** | Request line, headers, `Content-Length` body, incremental parsing | No `chunked`, no HTTP/2 |
| **Keep-Alive** | Max requests, idle timeout, `Connection` negotiation | — |
| **Static files** | Directory index, MIME detection, `/healthz`, path-traversal protection | Whole file is read into memory; no Range support |
| **MySQL (demo)** | Thread-local single connection, async execution, JSON rows, cross-thread write-back | Disabled by default; requires `--enable-sql`; read-only statement guard; no auth |
| **Unified status codes** | `StatusCode` across event loop, dispatchers, and startup | — |
| **Observability** | Atomic counters and a 5-second window log (QPS, connections, bytes, latency) | No `/metrics` endpoint |
| **Logging** | spdlog to the console | Level is fixed at `debug` in code |
| **JSON configuration** | nlohmann_json reads `config/SQLConfig.json` | Every retained field is wired; unimplemented fields were removed |
| **Containers** | Dockerfile + docker-compose (MySQL 8.0) | Runs as root; test data must be seeded manually |
| **Tests and benchmarks** | Smoke, HTTP edge cases, wrk scripts (static / Echo / SQL) | No automated database integration test |

---

## Architecture

```
Client
  │
  ▼
TcpServer (accept on main thread)
  │  round-robin
  ▼
IOThreadPool ──► EventLoop × N ──► Dispatcher (epoll / poll / select)
                                      │
                                      ▼
                              Channel (fd + callbacks)
                                      │
                                      ▼
                              TcpConnection (parse / write / Keep-Alive)
                                      │
                                      ▼
                              HttpRouter ──► /query, /sql → SqlHandler
                                      │            └─► SqlExecutor (worker thread)
                                      └─► everything else → StaticFileHandler
```

See [docs/architecture/ARCHITECTURE.md](./docs/architecture/ARCHITECTURE.md) for module
responsibilities and the thread model (the detailed docs are currently in Chinese).

---

## Requirements

| Item | Requirement | Notes |
|------|-------------|-------|
| OS | Linux / WSL2 | Uses `epoll`, `unistd.h`, etc.; **native Windows builds are not supported** |
| Compiler | GCC 11+ | C++20 required |
| Build | GNU Make 4.x (recommended) or CMake 3.16+ | Both are supported |
| MySQL | 5.7 / 8.0 (optional) | Only needed for `/sql` |
| Tests | curl, Python 3.10+, wrk | `run_http_cases.py` uses only the standard library; benchmarks need wrk |

All C++ dependencies (spdlog, nlohmann_json, MySQL Connector/C++) are vendored under
`third_party/`. No Conan, no system development packages.

---

## Quick start

```bash
# 1. Build (produces build/main_run and build/main_echo)
make clean && make -j"$(nproc)"

# 2. Start: <port> <static-root> [dispatcher] [threads] [conn-mode] ...
./build/main_run 8080 . epoll 4 keepalive 100 10000

# 3. Verify
curl -s http://127.0.0.1:8080/healthz          # -> ok
bash tests/test_smoke.sh http://127.0.0.1:8080/

# 4. HTTP edge cases (standard library only; no venv)
python3 tests/run_http_cases.py 127.0.0.1 8080
```

> `resource_path` is `chdir`-ed at startup, so the static root is relative to that argument.
> `config/SQLConfig.json`, however, is resolved from the **startup working directory**.
> See [docs/architecture/CONFIGURATION.md](./docs/architecture/CONFIGURATION.md) for details.

### CMake build (optional)

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j"$(nproc)"
# artifacts: build-cmake/main_run, build-cmake/main_echo
```

If `CMAKE_BUILD_TYPE` is omitted, `Release` is used. CMake and Make share the same sources and
`third_party/`.

### Using MySQL (optional)

The SQL endpoints are **not registered** by default. Static serving does not need a database
and does not read the SQL config.

1. Edit `config/SQLConfig.json`; at minimum set `host`, `user`, `password`, `database`.
2. Start with `--enable-sql`:
   ```bash
   ./build/main_run 8080 . epoll 4 keepalive 100 10000 --enable-sql
   ```
3. Open `http://127.0.0.1:8080/sql?sql=SELECT%201`.

> With `readOnly: true` (the template default), only a single
> `SELECT` / `SHOW` / `DESCRIBE` / `EXPLAIN` statement is allowed; writes return 403.
> Setting `readOnly: false` allows arbitrary SQL and logs a warning at startup.
>
> If the database is unreachable, the request returns 500 instead of hanging; if the SQL
> executor is unavailable, it returns 503.

---

## Configuration

The full configuration surface (CLI arguments, JSON fields, environment variables, path
semantics, startup validation) is documented in
[docs/architecture/CONFIGURATION.md](./docs/architecture/CONFIGURATION.md).

Command line:

```bash
./build/main_run <port> <resource_path> [dispatcher] [threads] [conn_mode] \
                 [keepalive_max_reqs] [keepalive_idle_ms] [--enable-sql]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `port` | required | Listen port |
| `resource_path` | required | Static root (the process `chdir`s into it) |
| `dispatcher` | `epoll` | `epoll` / `poll` / `select` |
| `threads` | `4` | I/O worker threads |
| `conn_mode` | `close` | `close` / `keepalive` |
| `keepalive_max_reqs` | `100` | Max requests per connection |
| `keepalive_idle_ms` | `10000` | Idle timeout in milliseconds |
| `--enable-sql` | off | Registers `/query` and `/sql`; reads `config/SQLConfig.json` at startup |

`config/SQLConfig.json` is an **empty-credentials template** (`host`/`database` are empty).
It is not read unless `--enable-sql` is set. With `--enable-sql`, startup validation runs and
rejects an empty `host`/`database` instead of starting with a broken configuration.

---

## HTTP API

| Method | Path | Behavior |
|--------|------|----------|
| GET | `/healthz` | Returns `ok\n`, 200 |
| GET | `/query?sql=...` | Registered only with `--enable-sql`; async SQL; 400/403/405/500 |
| GET | `/sql?sql=...` | Same as `/query`; 404 when SQL is disabled |
| GET | anything else | Static file or directory index; 403/404/405 |
| * | anything else | Static handler returns 405 |

Request body limit: **1 MB** (413). Parse timeout: **5 s** (408). Parameters, error semantics,
and examples: [docs/architecture/HTTP-API.md](./docs/architecture/HTTP-API.md).

### Security boundaries

- `/sql` is disabled by default; `--enable-sql` registers it, and it still has **no auth**;
- with `readOnly: true` (template default), only a single
  `SELECT` / `SHOW` / `DESCRIBE` / `EXPLAIN` is allowed; writes return 403.
  This is a heuristic HTTP-layer guard, not a complete SQL parser;
- `readOnly: false` allows arbitrary SQL and should only be used on a fully trusted host;
- the static handler rejects `config/`, `.git`, and any dot-path under the root
  (issue [#2](https://github.com/LYsnowQ/CPP_Reactor/issues/2)), but other source and docs
  files under the root are still downloadable; prefer a dedicated `public/` directory;
- there is no authentication, authorization, rate limiting, or TLS;
- conclusion: use it locally or on an isolated network only; **do not expose it to the internet**.

Config exposure and the read-only guard are tracked in
[#2](https://github.com/LYsnowQ/CPP_Reactor/issues/2) and
[#1](https://github.com/LYsnowQ/CPP_Reactor/issues/1) respectively.

---

## Tests and benchmarks

```bash
# Smoke
bash tests/test_smoke.sh http://127.0.0.1:8080/

# HTTP edge cases (standard library; no venv)
python3 tests/run_http_cases.py 127.0.0.1 8080

# SQL endpoint security (default-off, read-only guard; no real database needed)
bash tests/test_sql_security.sh

# Static-file benchmarks (M1-M5 / gradient)
bash tests/bench.sh static M
bash tests/bench.sh static GRADIENT

# Echo baseline (raw I/O ceiling)
bash tests/bench.sh echo ECHO

# SQL path validation (needs a database)
bash tests/bench_sql.sh . S

# Seed data (first time)
bash tests/setup.sh
tests/.venv/bin/python3 tests/seed_data.py --host 127.0.0.1 --user root --password '***' --database test
```

See [tests/README.md](./tests/README.md) for the full test/benchmark documentation and
[tests/BENCHMARK_REPORT.md](./tests/BENCHMARK_REPORT.md) for historical data.
CI builds the project, runs the smoke test and HTTP cases, and checks the docs; see
[.github/workflows/ci.yml](./.github/workflows/ci.yml).

---

## Docker

```bash
docker compose up -d --build
curl http://localhost:8080/healthz
docker compose logs -f app
docker compose down
```

At startup, `docker-entrypoint.sh` generates `config/SQLConfig.json` from environment variables:

| Variable | Default |
|----------|---------|
| `MYSQL_HOST` | `mysql` |
| `MYSQL_PORT` | `3306` |
| `MYSQL_USER` | `root` |
| `MYSQL_PASSWORD` | `root` |
| `MYSQL_DATABASE` | `test` |

> Compose starts MySQL and the app only; it **does not seed test data**. Run `seed_data.py`
> manually after startup if needed (see [tests/README.md](./tests/README.md)).
> The container runs as root and is intended for local demos only.

---

## Project layout

```
CPPReactor/
├── src/
│   ├── app/            main.cpp (HTTP server), main_echo.cpp (Echo baseline)
│   ├── core/           EventLoop, Buffer, Dispatcher and the three backends
│   ├── net/            TcpServer, TcpConnection, IOThreadPool, Channel
│   ├── protocol/       HttpRequest, HttpResponse, HttpRouter
│   ├── handler/        IHttpHandler, StaticFileHandler, SqlHandler
│   ├── persistence/    SqlConfig, MySqlConnection, SqlExecutor, transactions, thread-local connection
│   ├── repository/sql/ SqlRepository, SqlTask
│   ├── observability/  Metrics
│   └── utils/          JsonConfigLoader, StringUtils
├── include/            Headers mirroring src/
├── third_party/        spdlog, nlohmann_json, mysql-cppconn (vendored)
├── config/             SQLConfig.json (empty-credentials template)
├── tests/              Smoke / edge cases / benchmarks / seeding
├── docs/               Conventions, architecture, decisions, iterations, history, plan, reviews
├── scripts/            check_docs.py (documentation link checker)
├── .github/            CI, issue/PR templates
├── AGENTS.md           Entry point for AI agents and contributors
├── CHANGELOG.md        Version history
├── LICENSING.md        License boundaries (Chinese)
├── LICENSING.en.md     License boundaries (English)
├── LICENSE             Current license (PolyForm Noncommercial 1.0.0)
├── LICENSE-MIT         Historical MIT text (through be3bc0b)
├── CMakeLists.txt / Makefile
├── Dockerfile / docker-compose.yml / docker-entrypoint.sh
├── compile_flags.txt   Cross-platform clangd flags
└── .clang-format / .clangd / .gitignore / .dockerignore
```

---

## Performance baseline

Measured on 2026-06-19 on WSL2 (i7-13700H / 20 threads / 15 GiB, `-O2 -g`). These numbers are
specific to that machine and date:

| Scenario | QPS | Notes |
|----------|-----|-------|
| Echo (no HTTP parsing) | 1,781,742 | epoll, 12 threads, wrk with 800 connections |
| Static file (directory index) | 60,645 | epoll, 12 threads, 200 connections |
| SQL over the internet | ~15 | Public MySQL 5.7; connectivity check, not a benchmark |

The ~29x gap between Echo and HTTP comes from HTTP parsing, string handling, and file I/O.
Full data and reproduction commands: [tests/BENCHMARK_REPORT.md](./tests/BENCHMARK_REPORT.md).

---

## Documentation

The detailed docs are currently written in Chinese; the English README covers the essentials.

| Entry | Content |
|-------|---------|
| [docs/INDEX.md](./docs/INDEX.md) | Full documentation index |
| [docs/conventions/](./docs/conventions/README.md) | Mandatory conventions: docs, versioning, Git workflow, testing, review |
| [docs/architecture/](./docs/architecture/ARCHITECTURE.md) | Architecture, configuration, HTTP API |
| [docs/decisions/](./docs/decisions/README.md) | Architecture decision records (ADRs) |
| [docs/iterations/](./docs/iterations/README.md) | 29 historical iteration notes (historical only) |
| [docs/history/PROJECT-LOG.md](./docs/history/PROJECT-LOG.md) | Original project log (0.1.0–1.2.0) |
| [docs/plan/BACKLOG.md](./docs/plan/BACKLOG.md) | Migration note for GitHub Issues |
| [docs/reviews/2026-11-06-NORMALIZATION-AUDIT.md](./docs/reviews/2026-11-06-NORMALIZATION-AUDIT.md) | Documentation/configuration audit and plan |
| [AGENTS.md](./AGENTS.md) | First stop for AI agents and contributors |

---

## Version and license

Current version: **2.0.0**. Versioning follows SemVer; the source of truth is the git tags plus
[CHANGELOG.md](./CHANGELOG.md). Rules: [docs/conventions/VERSIONING.md](./docs/conventions/VERSIONING.md).

License:

- New code from 2026-11-06 is under **PolyForm Noncommercial 1.0.0** (source-available,
  noncommercial). Personal learning, noncommercial research, and **free redistribution** are
  allowed; commercial use requires a separate license.
- Historical versions (through `be3bc0b`, including tags `v1.0.0`–`v1.2.0`) remain **MIT**;
  the original text is in [LICENSE-MIT](./LICENSE-MIT).
- Full boundaries, the additional free-redistribution permission, and commercial licensing:
  [LICENSING.en.md](./LICENSING.en.md) (English) / [LICENSING.md](./LICENSING.md) (Chinese).
- The MIT grant cannot be revoked: previously published code remains usable under MIT.
