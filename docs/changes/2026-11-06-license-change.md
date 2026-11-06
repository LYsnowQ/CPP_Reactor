# 2026-11-06 许可变更（MIT → PolyForm Noncommercial）

- 类型：docs / legal
- 关联：[#5](https://github.com/LYsnowQ/CPP_Reactor/issues/5)、
  [ADR-0004](../decisions/0004-license-noncommercial.md)、[docs/legal/LICENSING.md](../legal/LICENSING.md)
- 前一个 MIT 提交：`be3bc0b`

## 背景

2026-11-06 收到 XERJ（[xerj.org](https://xerj.org)，Corpus Hub）的收录通知，拟将
commit `429dcaf` 以 MIT 收录，用于给 coding agent 提供“参考实现”。

核实结果：

- XERJ 是真实项目（[GitHub](https://github.com/xerj-org/xerj)），Corpus Hub 确实存在；
- 该邮件是**批量 outreach**：DEV 上有同一模板的收件帖，评论中也有其他收件人表示
  “Same here / I got the same message”；
- 邮件里的 “7 files / 380 lines” 与 `429dcaf` 的源码子集吻合（main.cpp、SqlHandler、
  SqlStatementGuard、两个测试文件），说明它按文件类型做了统计，但不代表逐篇深审；
- 收录动机是构建“参考代码语料”，与项目成熟度无关；有测试、有修复的 commit 正好是
  适合检索的样本。

## 决策

- 新代码：PolyForm Noncommercial 1.0.0；
- 附加许可：允许任何渠道**免费再分发**，不得收费或捆绑付费产品；
- 历史：截至 `be3bc0b`（含 tag `v1.0.0`–`v1.2.0`）继续 MIT，原文保留在 `LICENSES/MIT.txt`；
- 明确：MIT 不可撤回，`429dcaf` 仍可被按 MIT 使用。

## 文件

| 文件 | 变化 |
|------|------|
| `LICENSE` | 替换为 PolyForm Noncommercial 1.0.0 + Required Notice |
| `LICENSES/MIT.txt` | 新增，保存原 MIT 全文 |
| `docs/legal/LICENSING.md` | 新增，版本边界、允许/禁止用途、附加许可、商业授权 |
| `docs/decisions/0004-license-noncommercial.md` | 新增 ADR |
| `README.md` | 徽章与许可段落改为非商业；说明历史 MIT |
| `docs/INDEX.md`、`docs/decisions/README.md` | 索引更新 |
| `CHANGELOG.md` | Unreleased 记录许可变更 |

## 对 XERJ 的回复口径

**若选择不授权收录（推荐）：**

> Hi Ivan,
>
> Thanks for reaching out. Please don't include CPP_Reactor, or any of its commits, in the
> XERJ Corpus Hub. It's a personal learning project, and I don't consent to it being indexed
> or used in a commercial retrieval/training corpus. Please confirm removal and exclude
> future commits.
>
> Regards,
> LYsnowQ

**若允许非商业收录：**

> Hi Ivan,
>
> Thanks. Note that the repository is moving to a noncommercial license from this point on.
> The specific commit you referenced (`429dcaf`) was published under MIT, so that snapshot
> remains available under MIT; future commits are PolyForm Noncommercial 1.0.0 with a free
> redistribution permission. If the Corpus Hub is a commercial product, please exclude the
> project rather than rely on the MIT snapshot. If you include it, keep the attribution and
> the license notice, and tell me how removal requests are handled.

无论选哪种，都建议追问：语料是否用于模型训练、保留期限、删除机制、署名格式、
以及 Corpus Hub 是否属于商业产品。

## 验证

- `python3 scripts/check_docs.py` 通过；
- README 不再出现“MIT 开源”的现行声明；
- `docs/legal/LICENSING.md` 的边界与 tag 一致；
- 本变更不影响代码与 CI。

## 注意

- 这不是法律意见；PolyForm 条款的最终适用建议咨询律师；
- 更改许可证只影响未来提交，不能撤回已发布的 MIT 副本。
