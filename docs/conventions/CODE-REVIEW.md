# CODE-REVIEW.md — 自审与审查清单

单人项目没有外部 reviewer，但**每次提交前必须按本清单自审**；开 PR 时把核对结果写进描述。
审查的目标是找“会出事的点”，不是找风格偏好。

## 1. 通用

- [ ] 改动目的能用一句话说清，且提交里没有无关修改；
- [ ] 新增文件已纳入构建（Makefile 用 `find` 自动收集；CMake 需显式加入 `CORE_SOURCES`）；
- [ ] 没有提交真实凭据、内网地址、生成物、本机绝对路径；
- [ ] `make` 零警告；
- [ ] 测试与文档同步（见 [DOCUMENTATION.md](./DOCUMENTATION.md#5-新鲜度改代码时必须同步什么)）。

## 2. 并发与生命周期（最高风险）

- [ ] 跨线程回调只通过 `EventLoop::post()` 投递，不直接操作其他线程的 `Channel`/`TcpConnection`；
- [ ] 异步路径用 `weak_ptr`/`shared_from_this`，连接提前销毁时能安全退出；
- [ ] `conns_` 的插入/删除都在锁内，失败路径有回滚；
- [ ] 定时器/超时只在所属 EventLoop 线程读写；
- [ ] 没有在持锁时执行可能阻塞的 IO（数据库、文件）。

## 3. HTTP / 协议

- [ ] 状态码与 [HTTP-API.md](../architecture/HTTP-API.md) 一致；
- [ ] 解析失败路径清空 `inFlightRequest_`，且不会误判 Keep-Alive；
- [ ] 请求体上限、等待超时、连接关闭三者的顺序合理；
- [ ] 路径穿越校验在拼接后仍有效（`weakly_canonical` + 前缀比较）；
- [ ] 新增头/字段的默认值有文档。

## 4. SQL / 配置

- [ ] 新配置字段要么真正生效，要么在 `CONFIGURATION.md` 标注“未生效”，不允许静默忽略；
- [ ] 启动校验（`SqlConfig::validate`）是否被调用与文档一致；
- [ ] 数据库失败必须回到 HTTP 层（4xx/5xx），不能挂起、不能只写日志；
- [ ] 不把用户输入拼进 SQL 以外的系统调用；
- [ ] `readOnly` 等“安全承诺”如果没有实现，文档必须明确警示。

## 5. 构建 / 部署

- [ ] Make 与 CMake 都能构建 `main_run` 和 `main_echo`；
- [ ] 两种构建的优化级别与 RPATH 语义一致或已说明；
- [ ] Dockerfile 复制了所有运行时需要的文件（二进制、`third_party` 的 `.so`、`config/`）；
- [ ] compose 的端口、账号、数据卷与实际配置一致；
- [ ] CI 的步骤与本地验证命令一致。

## 6. 文档

- [ ] 没有新增 `file:///` 或 `C:\` 链接；
- [ ] 默认值、上限、路径与代码一致；
- [ ] 新文档已加入 [docs/INDEX.md](../INDEX.md)；
- [ ] 历史文档未被当作现行依据；
- [ ] `python3 scripts/check_docs.py` 通过。

## 7. 审查结论格式

在 PR 描述或提交评论中写：

```
结论：通过 / 有保留通过 / 退回
阻断项：无 / B-02（/sql 失败挂起）
已验证：编译、4 个 HTTP 用例、文档检查
未验证：真实 MySQL、Keep-Alive 超时
后续：B-05、B-07
```
