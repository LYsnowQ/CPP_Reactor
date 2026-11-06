# 2026-11-06 CI 与 main 分支治理

- 类型：ci / docs
- 关联：[#17](https://github.com/LYsnowQ/CPP_Reactor/issues/17)
- 基线：`main` @ `b174709`

## 改动

### CI

- `actions/checkout@v4` → `actions/checkout@v7`，消除 Node 20 弃用告警；
- 两个 job 保持不变：`Make + 冒烟 + HTTP 用例 + 文档`、`CMake 构建`。

### main 分支保护

在 GitHub 上启用：

| 规则 | 值 |
|------|-----|
| Require a pull request before merging | 是（0 个强制批准，单人项目） |
| Required status checks | `Make + 冒烟 + HTTP 用例 + 文档`、`CMake 构建` |
| Require branches to be up to date | 是 |
| Require linear history | 是 |
| Require conversation resolution | 是 |
| Allow force pushes | 否 |
| Allow deletions | 否 |
| Enforce for administrators | 否（保留应急绕过） |

`enforce_admins=false` 是单人项目的取舍：日常必须 PR，紧急情况下所有者仍可恢复；
如果以后有第二位维护者，应改为 `true` 并加至少 1 个批准。

## 验证

- [x] PR 上的两个 check 名称与保护规则一致；
- [x] `gh api repos/.../branches/main/protection` 返回上述规则；
- [x] CI 不再出现 Node 20 弃用告警；
- [x] 通过 PR 合并并删除分支。
