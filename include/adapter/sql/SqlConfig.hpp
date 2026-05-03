#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace reactor::adapter::sql
{
    enum class DriverKind : uint8_t
    {
        Unknown = 0,
        MysqlConnectorCpp
    };

    enum class ConnMode : uint8_t
    {
        Unknown = 0,
        ThreadLocalSingleConn
    };
    
    struct SqlConfig
    {
        //基本连接信息
        DriverKind driver = DriverKind::Unknown;
        std::string host;
        uint16_t port = 0;
        std::string user;
        std::string password;
        std::string database;
        std::string charset;
    
        //超时规则
        uint32_t connectTimeoutMs = 0;
        uint32_t readTimeoutMs = 0;
        uint32_t writeTimeoutMs = 0;

        //模型策略
        ConnMode connMode = ConnMode::Unknown;
        bool pingBeforeUse = false;
        uint32_t reconnectMaxAttempts = 0;
        uint32_t reconnectBackoffMs = 0;
        uint32_t maxConnLifetimeMs = 0;
        uint32_t maxConnIdleMs = 0;
    
        std::vector<std::string> initSqls;

        //观测和保护
        uint32_t slowQueryMs = 0;
        bool enableSqlLog = false;
        bool readOnly = false;

        //校验
        bool validate(std::string& err)const;
    };

    void from_json(const nlohmann::json& j, SqlConfig& cfg);
    void to_json(nlohmann::json& j, const SqlConfig& cfg);
    SqlConfig loadSqlConfigFromFileOrThrow(const std::filesystem::path& filePath);




}// reactor::adapter::sql
