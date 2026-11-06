# 2026-11-06 文档与配置规范化审计

- 审计对象：`cpp_reactor`（个人 C++ 学习项目）
- 参照对象：本机 `admin` 项目（多人协作、完整发布流程）
- 代码基线：`main` @ `483b7b1`（v1.2.0）
- 审计方式：逐文件阅读 + `git log` 回溯 + WSL2 (Ubuntu 22.04 / g++ 11.4) 实际编译与运行
- 结论：**项目代码可运行、结构清晰，但文档与配置层存在系统性漂移；已按“够用、可执行”的原则完成第一轮整改，并留下 12 条可追踪缺口。**

> **后续进展（2026-11-06 下午）**：B-01–B-04 已按 issue #1–#4 修复并合并，详见
> [p0/p1 修复记录](../changes/2026-11-06-p0-p1-fixes.md)。本文件保留审计时点的事实，
> 当前剩余缺口以 [BACKLOG](../plan/BACKLOG.md) 为准。

---

## 1. 摘要

| 维度 | 审计前 | 审计后 |
|------|--------|--------|
| 文档入口 | 根 README + 29 篇迭代 + 1 份日志，无索引 | README + AGENTS + CHANGELOG + docs/INDEX，四层入口 |
| 规范 | 无 | 6 份强制规范（定位/文档/版本/Git/测试/审查） |
| 现行事实 | 分散在 README 与迭代文档 | `docs/architecture/` 3 份，标注核对基线 |
| 历史材料 | 与现行说明混放 | `docs/history/iterations/`、`docs/history/`，显式“仅供追溯” |
| 版本记录 | 流水日志，无 tag、无 CHANGELOG | CHANGELOG + 本地 tag（v1.0.0/v1.1.0/v1.2.0） |
| 问题追踪 | 无 | BACKLOG 12 条（含证据/验收），GitHub issue 模板 |
| CI | 无 | 编译 + 冒烟 + HTTP 用例 + 文档检查 |
| 已复现缺陷 | 未记录 | 4 个（2 个已修，2 个登记） |

本次**修复**的关键项：README 的 Conan/迭代数量/能力描述失真、CMake `main_echo` 链接失败、
`.clangd` 绝对路径、HTTP 用例错误期望、Docker 无效数据挂载、`.gitignore` 残留。
本次**登记未修**的关键项：`/sql` 任意 SQL、配置可被静态下载、数据库失败挂起、配置字段大面积未生效。

---

## 2. 审计方法与证据

1. 阅读根目录全部文件、`docs/` 29 篇迭代、`tests/` 脚本与报告、`src/`/`include/` 关键实现；
2. 核对 README 的每条能力声明与代码（静态文件、日志、指标、SQL、Docker）；
3. 在 WSL2 中执行 `make clean && make -j8`、启动服务、跑冒烟与 4 个 HTTP 用例；
4. 用 `cmake -B cmake-build-test` 验证 CMake 路径；
5. 用空配置请求 `/sql`，确认失败路径行为；
6. 用 `curl` 验证静态根是否暴露配置文件；
7. 用 `git log`/`git tag` 核对版本与发布事实。

关键实测结果见 [§8 附录](#8-附录实测记录)。

---

## 3. 现状诊断

### 做得好的部分

- 分层清晰：`core/net/protocol/handler/persistence/repository` 职责基本单一；
- 迭代记录完整：29 篇文档保留了“为什么这样改”的上下文，这在学习项目中很少见；
- 压测方法规范：wrk、梯度、Echo 对照、环境记录，结论没有夸大；
- 依赖自包含：clone 后 `make` 即可编译，显著降低接手成本；
- 注释覆盖率高：头文件契约注释完整。

### 主要问题（按性质）

| 性质 | 表现 | 根因 |
|------|------|------|
| 文档过期 | README 仍写 Conan、27+ 迭代、分片传输、多 Sink | 改代码时没有“文档同步”这一环 |
| 说明错位 | 配置表把模板值当默认值；迭代结论与代码不符 | 文档没有“核对基线”和“事实来源”概念 |
| 配置错位 | `.clangd` 指向旧容器路径；CMake 漏链库；配置字段未接线 | 构建/配置有多份事实，彼此未校验 |
| 记录缺口 | 无 CHANGELOG、无 tag、无 issue/PR 模板 | 个人项目依赖记忆与提交信息 |
| 质量门禁缺失 | 无 CI、无文档检查、用例期望错误长期未发现 | 验证靠手工，且没有“必须绿”的入口 |

---

## 4. 从 admin 借鉴什么、放弃什么

| admin 做法 | 本项目处置 | 理由 |
|------------|-----------|------|
| `docs/conventions/` 强制规范 | **采纳并合并**为 6 篇 | 规则有效，但 20 篇对单人过重 |
| `docs/INDEX.md` + `scripts/docs-index.mjs` 自动生成 | **改造**为手工索引 + `check_docs.py` 链接检查 | 文档量小，生成器维护成本高于收益 |
| `docs/decisions/` ADR | **采纳** | 选型需要可追溯，3 条足够 |
| `docs/changes/` 变更记录 | **采纳** | 替代“每步写 notes”的轻量方案 |
| `docs/reviews/` 审计证据 | **采纳** | 本文件即第一份 |
| `docs/history/` 历史归档 | **采纳** | 29 篇迭代与旧日志有了正确归属 |
| issue 模板 + PR 模板 + issue-first | **采纳（降级）** | 非平凡改动必须可追溯；typo/单文档可直提 |
| CI（build/test/docs） | **采纳（精简）** | 去掉部署、预览、hotfix、branch-hygiene 等 |
| `main`/`stage` 双长期分支、rc/stable tag、发布对齐 | **放弃** | 无预发布环境与第二人验收 |
| `notes/<日期>/<用户>/` 逐步登记 | **放弃** | 单人项目噪音大；用 CHANGELOG + changes 代替 |
| 中英双语 `.en.md` | **放弃** | 当前读者为中文，翻译会先腐烂 |
| `AGENTS.md` 19KB + OMP/Claude 适配层 | **精简**为一份入口 | 只保留读序、约束、陷阱、流程 |
| GitLab/部署/环境隔离/备份等 ops 规范 | **放弃** | 本项目没有对应基础设施 |
| 自动化 issue-gate / PR-agent / 发版流水线 | **放弃** | 依赖多人协作与远端权限 |

原则：**保留“可验证的约束”，删除“需要额外人力维护的仪式”。**

---

## 5. 发现的问题清单

级别：p0 安全/不可用；p1 主要功能或构建受损；p2 一般质量；p3 优化。
处置：✅ 本次已修；📌 已登记 BACKLOG；📖 已用文档澄清；🗄 历史材料，保留原样。

### A. 文档结构、过期与错位

| ID | 问题 | 证据 | 级别 | 处置 |
|----|------|------|------|------|
| A1 | 29 篇迭代与根日志无分类、无索引，现行与历史混放 | `docs/` 目录 | p1 | ✅ 迁移到 `history/iterations/`、`history/`，新增 INDEX |
| A2 | README 目录含“使用 CMake + Conan”，正文无此章节 | `README.md` 旧 21 行 | p1 | ✅ 重写 |
| A3 | README 项目结构列出已删除的 `conanfile.py` | 旧 454 行；1.1.0 已删除 | p1 | ✅ 重写 |
| A4 | README 称“27+ 个迭代文档”，实际 29 | 旧 446 行 | p2 | ✅ 重写 |
| A5 | README 配置表把模板值写成默认值，且漏 `driver`/`initSqls` | 旧 188–207 行 vs `SqlConfig.hpp` | p1 | ✅ 拆到 CONFIGURATION，逐字段标注 |
| A6 | 8 篇迭代共 78 行使用 `file:///project/CPPReactor/...` 绝对链接 | 全文检索 | p2 | 📖 历史保留 + 阅读须知（不改写历史） |
| A7 | 迭代 23 链接已迁移的 `scripts/seed_data.py` | `ITERATION_23` 旧 20 行 | p3 | 📖 同 A6 |
| A8 | 迭代 29 声称移除 `response_`/`asyncPending_`，实际仍在且被使用 | `ITERATION_29` vs `TcpConnection.hpp:129,133`、`TcpConnection.cpp:274` | p2 | 📖 在 CHANGELOG/迭代索引中标注 |
| A9 | README 未记录真实接口（`/healthz`、`/query`、`/sql`、403/405/408/413） | 旧 README | p1 | ✅ 新增 HTTP-API |
| A10 | 没有任何安全边界说明 | 旧 README | p0 | ✅ README + HTTP-API + ADR-0003 |
| A11 | 现行文档无“核对基线/最后核对日期” | 全仓库 | p2 | ✅ 新文档均带基线；规范要求维护 |
| A12 | `docs/spec/`、`docs/learning/` 被 gitignore，但目录不存在 | `.gitignore` 旧 50–51 行 | p3 | ✅ 删除忽略项（避免隐藏未来文档） |

### B. 配置错位与失效

| ID | 问题 | 证据 | 级别 | 处置 |
|----|------|------|------|------|
| B1 | `.clangd` 硬编码 `/project/CPPReactor/...`，当前路径完全不符 | `.clangd` 旧 4–7 行 | p1 | ✅ 改为 `compile_flags.txt` 相对路径 |
| B2 | CMake `main_echo` 未链接 MySQL 库，构建失败 | 实测 `undefined reference to sql::mysql::_get_driver_instance_by_name` | p1 | ✅ 补链接 |
| B3 | CMake 未声明 VERSION；未指定构建类型时无优化，与 Make `-O2` 不一致 | `CMakeLists.txt` | p2 | ✅ `VERSION 1.2.0` + 默认 Release |
| B4 | CMake RPATH 为绝对源码路径，产物不可移动；Make 用 `$ORIGIN` | `CMakeLists.txt` 旧 78–81 行 | p2 | ✅ 统一为 `$ORIGIN` |
| B5 | `SQLConfig` 11 个字段只解析不生效（`readOnly` 含安全语义） | `CONFIGURATION.md` 字段表 | p1 | 📌 B-04（`readOnly` 另见 B-01） |
| B6 | 启动链路未调用 `SqlConfig::validate`，空 host 也能启动 | `main.cpp:72-85` | p1 | 📌 B-04 |
| B7 | compose 把 `seed_data.py` 挂到 `/docker-entrypoint-initdb.d/`，MySQL 不执行 `.py` | `docker-compose.yml:15` | p2 | ✅ 删除无效挂载并说明 |
| B8 | compose 创建了未被使用的 `user` 账号，应用用 root | `docker-compose.yml:9-10,32-33` | p3 | ✅ 统一为 root（与文档一致） |
| B9 | `.gitignore` 残留 Conan 条目、重复 `build/` | `.gitignore` 旧 13–20、39 行 | p3 | ✅ 清理 |
| B10 | Docker 构建上下文包含 `docs/`、`tests/`、`.github/` | `.dockerignore` | p3 | ✅ 排除 |
| B11 | 容器以 root 运行、无健康检查 | `Dockerfile` | p2 | 📌 B-05 |
| B12 | 静态根包含 `config/`，凭据可被下载 | 实测 `/config/SQLConfig.json` → 200 | p0 | 📌 B-02 + 文档警告 |

### C. 说明与实现不一致

| ID | 声明 | 实际 | 级别 | 处置 |
|----|------|------|------|------|
| C1 | “支持大文件分片传输” | 整文件读入 `std::string` | p2 | ✅ 改为“整文件读入、无 Range” |
| C2 | 日志“支持多 Sink” | 只有默认控制台 sink | p2 | ✅ 删除该声明 |
| C3 | “慢查询日志” | `slowQueryMs` 未使用 | p2 | ✅ 删除，登记 B-07 |
| C4 | “Metrics 采集” | 有原子计数器，但只每 5 秒打日志，无导出 | p3 | ✅ 明确为“窗口日志” |
| C5 | 迭代 29 的“移除死成员” | 成员仍存在 | p2 | ✅ 历史标注 |
| C6 | Docker“一键部署” | 应用可起，但不注入数据 | p2 | ✅ 明确“需手动注入” |
| C7 | 测试“HTTP 边界用例通过” | 实测 2/4 失败 | p1 | ✅ 修正用例（见 §6） |
| C8 | `readOnly` 是安全开关 | 完全未执行 | p0 | ✅ 文档警示 + B-01 |

### D. 版本与记录缺口

| ID | 问题 | 处置 |
|----|------|------|
| D1 | 无 CHANGELOG，版本变化只存在于流水日志 | ✅ 新增，覆盖 0.x–1.2.0 |
| D2 | 无 git tag，无法定位“发布了什么” | ✅ 补打 3 个本地 tag，未推送 |
| D3 | 无单一版本来源 | ✅ 统一为 tag + CHANGELOG + CMake `project(VERSION)` |
| D4 | 0.11.0–0.14.0 无任何记录 | 📖 CHANGELOG 注明无法回溯 |
| D5 | 无 issue/PR 记录 | ✅ 模板 + 规范；**不补历史 issue**（见 §7.2） |

### E. 工程实践缺口

| ID | 问题 | 处置 |
|----|------|------|
| E1 | 无 CI | ✅ `.github/workflows/ci.yml` |
| E2 | 无 issue/PR 模板 | ✅ `.github/` |
| E3 | 无文档断链检查 | ✅ `scripts/check_docs.py` |
| E4 | 无单元测试 | 📌 B-09 |
| E5 | 无优雅停机 | 📌 B-06 |
| E6 | 无 AI/协作者入口 | ✅ `AGENTS.md` |
| E7 | 用例只覆盖 4 条路径 | 📌 B-09 |
| E8 | 无 Release 说明 | ✅ 模板与流程（VERSIONING） |

---

## 6. 本次已落实的整改

### 文档

- 迁移：`docs/ITERATION_*.md` → `docs/history/iterations/`（29 篇；当时为 `docs/iterations/`）；`项目日志.md` →
  `docs/history/PROJECT-LOG.md`；
- 新增：`AGENTS.md`、`CHANGELOG.md`、`docs/INDEX.md`、6 篇规范、3 篇架构、3 条 ADR、
  BACKLOG、本审计、变更记录；
- 重写：`README.md`（605 行 → 约 330 行，去掉与代码不符的性能长文与重复配置表）；
- 历史文档加“仅供追溯”提示，保留原始 `file:///` 链接不修改。

### 构建与配置

- `.clangd` 去掉绝对路径；新增 `compile_flags.txt`；
- `CMakeLists.txt`：`VERSION 1.2.0`、默认 Release、`main_echo` 补 MySQL 链接、RPATH 改相对；
- `docker-compose.yml`：删除无效 initdb 挂载，统一账号；
- `.gitignore`/`.dockerignore` 清理。

### 测试

`tests/run_http_cases.py` 的两处问题：

1. **413 用例**：服务端在客户端仍在发送 1 MB body 时回 413 并关闭，客户端 `recv` 收到
   `ECONNRESET`，原实现丢弃已读字节 → 误判失败。现保留已读字节再判断。
2. **分片 Body 用例**：原用 `POST /x` 期望 200，但静态处理器对非 GET 正确返回 405 →
   期望值本身错误。现改为 `GET /`（带分片 body），既验证增量解析，也符合路由语义。

> 这是**用例缺陷**，不是服务器行为变更。服务器在 413 时可能触发 RST 的健壮性问题仍存在，
> 但客户端现在能正确识别 413。

### 版本

- 补打本地 annotated tag：`v1.0.0` → `77e1e44`、`v1.1.0` → `af11b30`、`v1.2.0` → `483b7b1`；
- 未推送，未创建 GitHub Release。

---

## 7. 规范化后的运行机制

### 7.1 一次改动的最小闭环

```
确认分支 → 查/建 issue（非平凡）→ 改代码
   → 按 DOCUMENTATION 新鲜度表同步文档
   → make + 冒烟 + HTTP 用例 + check_docs
   → CHANGELOG [Unreleased]
   → 提交（type(scope): 目的）
   → squash 回 main
```

### 7.2 关于 issue 与历史补录

- **不补历史 issue**：24 个提交、29 篇迭代已经构成完整叙事，补建 24 个 issue 只会产生噪音，
  且无法还原当时的验收条件。
- **从下一次非平凡改动开始 issue-first**：BACKLOG 的 12 条已按 issue 结构写好，可直接创建。
- 纯文档、typo、发版提交不强制 issue。
- 所有者已确认；本次为 p0/p1 创建了 [#1–#4](https://github.com/LYsnowQ/CPP_Reactor/issues)，
  p2/p3 仍在 BACKLOG，暂不建单。

### 7.3 门禁（必须通过）

| 门禁 | 方式 | 阻断 |
|------|------|------|
| 编译 | CI `make` | 是 |
| 冒烟 + HTTP 用例 | CI 启动服务后执行 | 是 |
| 文档链接 | `scripts/check_docs.py` | 是 |
| CHANGELOG | 人工检查（非平凡改动） | PR 层面 |
| 配置一致性 | CODE-REVIEW 清单第 4 节 | 人工 |

### 7.4 巡检节奏

- **每次提交**：文档同步 + 链接检查；
- **每个版本**：README/CONFIGURATION/HTTP-API/CHANGELOG 四处核对；
- **每季度**：抽 5 篇现行文档做“命令可跑、默认值一致、链接有效”验证，记入 `docs/reviews/`。

---

## 8. 分期计划

### 第一阶段（本次，已完成）

文档重组、README 纠偏、构建/配置修复、测试修正、CI/模板、版本补录。

### 第二阶段（建议 1–2 周内，p0/p1）

> 状态：**2026-11-06 已完成**（#1–#4）；下表保留当时的排序依据。

| 顺序 | 事项 | 预计 |
|------|------|------|
| 1 | B-01 `/sql` 写语句拒绝或默认关闭 | 0.5 天 |
| 2 | B-02 静态根拒绝 `config/` 与隐藏文件 | 0.5 天 |
| 3 | B-03 数据库失败回写 500 + 用例 | 0.5 天 |
| 4 | B-04 配置字段接线或删除 + 启动校验 | 1 天 |

> 建议顺序：先堵住“任意 SQL”和“配置可下载”，再修挂起，最后处理配置一致性。
> 每项都应先开 issue，再改代码。

### 第三阶段（p2）

B-05 Docker 账号/非 root、B-06 优雅停机、B-07 日志与指标、B-08 流式静态文件、B-09 测试补齐。

### 第四阶段（p3）

B-10 自动基准、B-11 参数校验、B-12 `--help/--version`。

---

## 9. 不采纳的 admin 规则（再次明确）

- 双长期分支与 rc/stable 发布体系：没有预发布环境；
- 每步 notes 登记：单人项目收益低；
- 中英双语：翻译会先于代码腐烂；
- 自动 issue 生命周期、PR Agent、部署流水线：依赖多人协作与远端权限；
- 运维/备份/GitLab/环境隔离：本项目无对应基础设施。

---

## 10. 验收清单

- [x] README 每条能力声明都能在代码中找到依据；
- [x] 所有现行文档使用相对链接，`check_docs.py` 通过；
- [x] 历史文档有“仅供追溯”标识；
- [x] Make 与 CMake 均能构建两个目标；
- [x] HTTP 用例 4/4 通过；
- [x] 版本有 tag + CHANGELOG + CMake VERSION 三处一致；
- [x] 已知问题有 ID、证据、验收条件；
- [x] p0/p1 已建远端 issue（#1–#4）；
- [x] CI 覆盖编译、冒烟、HTTP 用例、文档检查；
- [ ] 真实 MySQL 链路验证（需本机数据库，未执行）；
- [ ] 远端 CI 首跑（需推送后确认）。

---

## 11. 风险与代价

| 风险 | 说明 | 缓解 |
|------|------|------|
| 旧链接失效 | GitHub 上指向 `docs/ITERATION_*.md` 的书签会 404 | git 历史仍可追溯；如需兼容可加跳转文件（未做） |
| 规范过重 | 6 篇规范可能再次腐烂 | 每篇都很短；季度巡检；不强制仪式 |
| 历史文档误导 | 迭代文档含失效路径/结论 | 页首声明 + 索引提示 + 现行文档不引用 |
| tag 误用 | 本地 tag 未推送，GitHub Release 不存在 | 文档明确“未推送”，推送需所有者确认 |
| 测试修正被误解为“改测试过关” | 实际是期望值错误 | 变更记录与 §6 说明原因，服务器行为未改 |

---

## 12. 附录：实测记录

环境：WSL2 Ubuntu 22.04.5 / g++ 11.4.0 / 20 线程 / commit `483b7b1`。

```text
# 1. Make 构建
$ make clean && make -j8
结果：成功，warning 计数 0

# 2. HTTP 用例（修正前）
[PASS] valid_get -> 200
[PASS] invalid_request_line -> 400
[FAIL] payload_too_large exception=[Errno 104] Connection reset by peer
[FAIL] split_content_length_body expected=200 got=405
Summary: total=4 failed=2 passed=2

# 3. HTTP 用例（修正后）
[PASS] payload_too_large -> HTTP/1.1 413 Payload Too Large
[PASS] split_content_length_body -> HTTP/1.1 200 OK
Summary: total=4 failed=0 passed=4

# 4. CMake（修正前）
/usr/bin/ld: undefined reference to `sql::mysql::_get_driver_instance_by_name(char const*)'
gmake: *** [CMakeFiles/main_echo.dir/build.make:468: main_echo] Error 1

# 5. /sql 数据库不可达
$ curl --max-time 8 'http://127.0.0.1:18081/query?sql=SELECT%201'
结果：8 秒超时，无响应
日志：[warning] SqlExecutor job failed: sql connect failed: Unable to connect to :3306

# 6. 静态根暴露配置
$ curl -o /dev/null -w '%{http_code}' http://127.0.0.1:18084/config/SQLConfig.json
200
```
