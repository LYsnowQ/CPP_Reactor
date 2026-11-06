#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace reactor::persistence
{

/// @brief 数据库连接配置模型
///
/// 只保留**真正生效**的字段：连接信息、三组超时、连接管理选项与 HTTP 只读护栏。
/// 历史配置中的 driver / connMode / reconnect* / initSqls / slowQueryMs / enableSqlLog
/// 已被移除；反序列化会忽略这些多余键，不会报错。
///
/// 校验规则：
///   - host/user/database/charset 不能为空
///   - port 必须大于 0
///   - 三组超时必须大于 0
///   - maxConnLifetimeMs / maxConnIdleMs 必须大于 0，且 idle ≤ lifetime
struct SqlConfig
{
    // 基本连接信息
    std::string host;
    uint16_t port = 0;
    std::string user;
    std::string password;
    std::string database;
    std::string charset;

    // 超时规则
    uint32_t connectTimeoutMs = 0;
    uint32_t readTimeoutMs = 0;
    uint32_t writeTimeoutMs = 0;

    // 连接管理
    bool pingBeforeUse = false;
    uint32_t maxConnLifetimeMs = 0;
    uint32_t maxConnIdleMs = 0;

    // HTTP 端点只读护栏（仅影响 /sql、/query）
    bool readOnly = false;

    /// @brief 校验配置字段是否合法
    /// @param err 输出参数，校验失败时描述原因
    /// @return true 校验通过，false 校验失败
    bool validate(std::string &err) const;
};

/// @brief JSON → SqlConfig 反序列化
void from_json(const nlohmann::json &j, SqlConfig &cfg);

/// @brief SqlConfig → JSON 序列化
void to_json(nlohmann::json &j, const SqlConfig &cfg);

/// @brief 从文件加载 SQL 配置（含校验）
SqlConfig loadSqlConfigFromFileOrThrow(const std::filesystem::path &filePath);

} // namespace reactor::persistence
