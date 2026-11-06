# 2026-11-06 英文 README 与英文许可摘要

- 类型：docs
- 关联：issue [#22](https://github.com/LYsnowQ/CPP_Reactor/issues/22)
- 基线：`main` @ `2.0.0`

## 目的

为海外读者提供可直接使用的英文入口，同时避免英文文档滞后于 2.0.0 的实际情况。

## 交付

- `README.en.md`：与中文 README 对齐的英文版本；
- `LICENSING.en.md`：英文许可摘要（PolyForm Noncommercial、历史 MIT、免费再分发附加许可、
  商业授权入口）；
- 中英文 README / LICENSING 顶部互加语言链接；
- `docs/INDEX.md` 收录英文入口；
- 中文 README 的项目结构补充英文文件。

## 保证一致性的点

- 版本为 **2.0.0**，不是 1.2.0；
- `/query`、`/sql` 默认关闭，需要 `--enable-sql`；
- `SQLConfig` 保留字段全部生效，未实现字段已移除；
- 只读护栏是启发式，不等于数据库只读账号；
- 静态文件拒绝 `config/`、`.git`、点文件，但其他源码仍可下载；
- 许可证为 PolyForm Noncommercial，历史版本仍 MIT；
- 测试命令与 CI 配置保持当前状态。

## 未翻译的部分

`docs/conventions/`、`docs/architecture/` 等详细文档仍为中文；英文 README 明确写了这一点，
避免读者误以为存在完整英文文档。后续如果需要，可按同样流程逐篇补 `.en.md`。

## 验证

- `python3 scripts/check_docs.py` 通过；
- 所有英文 README 中的相对链接存在；
- 通过 PR 合并并删除分支。
