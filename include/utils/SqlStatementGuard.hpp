#pragma once

#include <string_view>

namespace reactor::utils
{

/// @brief 粗粒度的只读语句判定
///
/// 用于 HTTP SQL 演示端点的默认护栏：只允许以 SELECT / SHOW / DESCRIBE / DESC /
/// EXPLAIN 开头的单条语句。
///
/// 这是**防御性启发式**，不是完整的 SQL 解析器：
///   - 拒绝任何 `--`、`#`、`/*` 注释序列，避免用注释拼接关键字；
///   - 拒绝分号出现在结尾之外的情况，阻断多语句；
///   - 拒绝会写文件或加锁的 SELECT 形式：`INTO OUTFILE`、`INTO DUMPFILE`、
///     `FOR UPDATE`、`FOR SHARE`、`LOCK IN SHARE MODE`、`LOAD_FILE()`、
///     `SLEEP()`、`BENCHMARK()`、`GET_LOCK()`、`RELEASE_LOCK()`；
///   - 字符串字面量中的分号或上述关键字会被误判（保守失败）。
///
/// 真正的安全边界仍应是数据库只读账号 + 网络隔离。
///
/// @param sql 已 URL 解码的 SQL 文本
/// @return true 表示判定为只读语句
/// @thread safe（无全局状态）
bool isReadOnlyStatement(std::string_view sql);

} // namespace reactor::utils
