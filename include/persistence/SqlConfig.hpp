#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace reactor::persistence
{

/// @brief 数据库驱动类型
enum class DriverKind : uint8_t
{
    Unknown = 0,
    MysqlConnectorCpp
};

/// @brief 连接管理策略
enum class ConnMode : uint8_t
{
    Unknown = 0,
    ThreadLocalSingleConn
};

/// @brief 数据库连接配置模型
///
/// 包含连接参数、超时规则、连接策略、初始化 SQL 和观测配置。
/// 支持 JSON 序列化/反序列化（from_json/to_json）和字段校验（validate）。
///
/// 校验规则：
///   - driver/connMode 不能为 Unknown
///   - host/user/database/charset 不能为空
///   - port/超时/连接生命周期必须大于 0
///   - maxConnIdleMs ≤ maxConnLifetimeMs
///   - initSqls 非空且每项非空
struct SqlConfig
{
    // 基本连接信息
    DriverKind driver = DriverKind::Unknown;
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

    // 模型策略
    ConnMode connMode = ConnMode::Unknown;
    bool pingBeforeUse = false;
    uint32_t reconnectMaxAttempts = 0;
    uint32_t reconnectBackoffMs = 0;
    uint32_t maxConnLifetimeMs = 0;
    uint32_t maxConnIdleMs = 0;

    std::vector<std::string> initSqls;

    // 观测和保护
    uint32_t slowQueryMs = 0;
    bool enableSqlLog = false;
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
