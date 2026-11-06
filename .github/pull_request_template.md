## 目的

<!-- 一句话说明这次改动解决什么问题；非平凡改动请关联 issue / BACKLOG ID。 -->

Refs:

## 改动点

-

## 验证

<!-- 按 CONVENTIONS/TESTING 的格式写：环境、命令、结果、未验证项。 -->

```text
环境：
命令：
结果：
未验证：
```

## 文档

- [ ] 受影响的现行文档已更新（`docs/architecture/`、README 等）
- [ ] `CHANGELOG.md` 的 `[Unreleased]` 已补
- [ ] `python3 scripts/check_docs.py` 通过
- [ ] 历史文档未被改写

## 检查

- [ ] `make` 零错误零警告
- [ ] 冒烟 + HTTP 用例通过
- [ ] 没有提交凭据、生成物、绝对路径
- [ ] 若改变配置/接口，已同步 `CONFIGURATION.md` / `HTTP-API.md`
- [ ] 若引入技术债，已登记 `docs/plan/BACKLOG.md`

## 风险与回滚

<!-- 影响范围、可能的回归、如何回滚（revert / 配置回退 / 重建镜像）。 -->
