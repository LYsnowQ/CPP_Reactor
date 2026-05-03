#include "adapter/sql/SqlConfig.hpp"
#include "utils/JsonConfigLoader.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace reactor::adapter::sql
{
namespace
{
bool isBlank_(const std::string& s)
{
    return std::all_of(
        s.begin(),
        s.end(),
        [](unsigned char ch)
        {
            return std::isspace(ch) != 0;
        });
}

DriverKind parseDriverKind_(const std::string& value)
{
    if(value == "mysql_connector_cpp")
    {
        return DriverKind::MysqlConnectorCpp;
    }
    throw std::invalid_argument("driver 非法，当前仅支持: mysql_connector_cpp");
}

ConnMode parseConnMode_(const std::string& value)
{
    if(value == "thread_local_single_conn")
    {
        return ConnMode::ThreadLocalSingleConn;
    }
    throw std::invalid_argument("connMode 非法，当前仅支持: thread_local_single_conn");
}

const char* driverKindToString_(DriverKind value)
{
    switch(value)
    {
    case DriverKind::MysqlConnectorCpp:
        return "mysql_connector_cpp";
    default:
        return "unknown";
    }
}

const char* connModeToString_(ConnMode value)
{
    switch(value)
    {
    case ConnMode::ThreadLocalSingleConn:
        return "thread_local_single_conn";
    default:
        return "unknown";
    }
}
} // namespace

bool SqlConfig::validate(std::string& err) const
{
    err.clear();

    if(driver == DriverKind::Unknown)
    {
        err = "driver 不能为空且必须是受支持值";
        return false;
    }
    if(connMode == ConnMode::Unknown)
    {
        err = "connMode 不能为空且必须是受支持值";
        return false;
    }
    if(isBlank_(host))
    {
        err = "host 不能为空";
        return false;
    }
    if(port == 0)
    {
        err = "port 必须在 1~65535 之间";
        return false;
    }
    if(isBlank_(user))
    {
        err = "user 不能为空";
        return false;
    }
    if(isBlank_(database))
    {
        err = "database 不能为空";
        return false;
    }
    if(isBlank_(charset))
    {
        err = "charset 不能为空";
        return false;
    }
    if(connectTimeoutMs == 0 || readTimeoutMs == 0 || writeTimeoutMs == 0)
    {
        err = "connect/read/write timeout 必须大于 0";
        return false;
    }
    if(reconnectMaxAttempts > 0 && reconnectBackoffMs == 0)
    {
        err = "reconnectBackoffMs 不能为 0（当 reconnectMaxAttempts > 0 时）";
        return false;
    }
    if(maxConnLifetimeMs == 0)
    {
        err = "maxConnLifetimeMs 必须大于 0";
        return false;
    }
    if(maxConnIdleMs == 0)
    {
        err = "maxConnIdleMs 必须大于 0";
        return false;
    }
    if(maxConnIdleMs > maxConnLifetimeMs)
    {
        err = "maxConnIdleMs 不能大于 maxConnLifetimeMs";
        return false;
    }
    if(slowQueryMs == 0)
    {
        err = "slowQueryMs 必须大于 0";
        return false;
    }
    if(initSqls.empty())
    {
        err = "initSqls 不能为空";
        return false;
    }

    for(size_t i = 0; i < initSqls.size(); ++i)
    {
        if(isBlank_(initSqls[i]))
        {
            err = "initSqls 存在空 SQL，索引=" + std::to_string(i);
            return false;
        }
    }
    return true;
}

void from_json(const nlohmann::json& j, SqlConfig& cfg)
{
    cfg.driver = parseDriverKind_(j.at("driver").get<std::string>());
    cfg.host = j.at("host").get<std::string>();
    cfg.port = j.at("port").get<uint16_t>();
    cfg.user = j.at("user").get<std::string>();
    cfg.password = j.at("password").get<std::string>();
    cfg.database = j.at("database").get<std::string>();
    cfg.charset = j.at("charset").get<std::string>();

    cfg.connectTimeoutMs = j.at("connectTimeoutMs").get<uint32_t>();
    cfg.readTimeoutMs = j.at("readTimeoutMs").get<uint32_t>();
    cfg.writeTimeoutMs = j.at("writeTimeoutMs").get<uint32_t>();

    cfg.connMode = parseConnMode_(j.at("connMode").get<std::string>());
    cfg.pingBeforeUse = j.at("pingBeforeUse").get<bool>();
    cfg.reconnectMaxAttempts = j.at("reconnectMaxAttempts").get<uint32_t>();
    cfg.reconnectBackoffMs = j.at("reconnectBackoffMs").get<uint32_t>();
    cfg.maxConnLifetimeMs = j.at("maxConnLifetimeMs").get<uint32_t>();
    cfg.maxConnIdleMs = j.at("maxConnIdleMs").get<uint32_t>();

    cfg.initSqls = j.at("initSqls").get<std::vector<std::string>>();

    cfg.slowQueryMs = j.at("slowQueryMs").get<uint32_t>();
    cfg.enableSqlLog = j.at("enableSqlLog").get<bool>();
    cfg.readOnly = j.at("readOnly").get<bool>();
}

void to_json(nlohmann::json& j, const SqlConfig& cfg)
{
    j = nlohmann::json{
        {"driver", driverKindToString_(cfg.driver)},
        {"host", cfg.host},
        {"port", cfg.port},
        {"user", cfg.user},
        {"password", cfg.password},
        {"database", cfg.database},
        {"charset", cfg.charset},
        {"connectTimeoutMs", cfg.connectTimeoutMs},
        {"readTimeoutMs", cfg.readTimeoutMs},
        {"writeTimeoutMs", cfg.writeTimeoutMs},
        {"connMode", connModeToString_(cfg.connMode)},
        {"pingBeforeUse", cfg.pingBeforeUse},
        {"reconnectMaxAttempts", cfg.reconnectMaxAttempts},
        {"reconnectBackoffMs", cfg.reconnectBackoffMs},
        {"maxConnLifetimeMs", cfg.maxConnLifetimeMs},
        {"maxConnIdleMs", cfg.maxConnIdleMs},
        {"initSqls", cfg.initSqls},
        {"slowQueryMs", cfg.slowQueryMs},
        {"enableSqlLog", cfg.enableSqlLog},
        {"readOnly", cfg.readOnly},
    };
}

SqlConfig loadSqlConfigFromFileOrThrow(const std::filesystem::path& filePath)
{
    return reactor::utils::config::loadConfigAndValidateOrThrow<SqlConfig>(filePath);
}
} // namespace reactor::adapter::sql
