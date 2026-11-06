#include "persistence/SqlConfig.hpp"
#include "utils/JsonConfigLoader.hpp"

#include <algorithm>
#include <cctype>

namespace reactor::persistence
{
    namespace
    {
        bool isBlank_(const std::string &s)
        {
            return std::all_of(s.begin(), s.end(),
                               [](unsigned char ch) { return std::isspace(ch) != 0; });
        }
    } // namespace

    // ====================================================================
    // 配置校验
    // ====================================================================
    //
    // 逐字段检查，失败即返回（短路），err 中包含首个失败原因。
    // 这里只校验“真正生效”的字段，避免配置看起来合法但行为缺失。
    bool SqlConfig::validate(std::string &err) const
    {
        err.clear();

        if (isBlank_(host))
        {
            err = "host 不能为空";
            return false;
        }
        if (port == 0)
        {
            err = "port 必须在 1~65535 之间";
            return false;
        }
        if (isBlank_(user))
        {
            err = "user 不能为空";
            return false;
        }
        if (isBlank_(database))
        {
            err = "database 不能为空";
            return false;
        }
        if (isBlank_(charset))
        {
            err = "charset 不能为空";
            return false;
        }
        if (connectTimeoutMs == 0 || readTimeoutMs == 0 || writeTimeoutMs == 0)
        {
            err = "connect/read/write timeout 必须大于 0";
            return false;
        }
        if (maxConnLifetimeMs == 0)
        {
            err = "maxConnLifetimeMs 必须大于 0";
            return false;
        }
        if (maxConnIdleMs == 0)
        {
            err = "maxConnIdleMs 必须大于 0";
            return false;
        }
        if (maxConnIdleMs > maxConnLifetimeMs)
        {
            err = "maxConnIdleMs 不能大于 maxConnLifetimeMs";
            return false;
        }
        return true;
    }

    // ====================================================================
    // JSON 反序列化
    // ====================================================================
    //
    // 使用 j.at(key) 而非 j.value(key, default)，确保缺失必填字段时抛出异常。
    // 旧配置中的未知键会被 nlohmann::json 忽略。
    void from_json(const nlohmann::json &j, SqlConfig &cfg)
    {
        cfg.host = j.at("host").get<std::string>();
        cfg.port = j.at("port").get<uint16_t>();
        cfg.user = j.at("user").get<std::string>();
        cfg.password = j.at("password").get<std::string>();
        cfg.database = j.at("database").get<std::string>();
        cfg.charset = j.at("charset").get<std::string>();

        cfg.connectTimeoutMs = j.at("connectTimeoutMs").get<uint32_t>();
        cfg.readTimeoutMs = j.at("readTimeoutMs").get<uint32_t>();
        cfg.writeTimeoutMs = j.at("writeTimeoutMs").get<uint32_t>();

        cfg.pingBeforeUse = j.at("pingBeforeUse").get<bool>();
        cfg.maxConnLifetimeMs = j.at("maxConnLifetimeMs").get<uint32_t>();
        cfg.maxConnIdleMs = j.at("maxConnIdleMs").get<uint32_t>();

        cfg.readOnly = j.at("readOnly").get<bool>();
    }

    void to_json(nlohmann::json &j, const SqlConfig &cfg)
    {
        j = nlohmann::json{
            {"host", cfg.host},
            {"port", cfg.port},
            {"user", cfg.user},
            {"password", cfg.password},
            {"database", cfg.database},
            {"charset", cfg.charset},
            {"connectTimeoutMs", cfg.connectTimeoutMs},
            {"readTimeoutMs", cfg.readTimeoutMs},
            {"writeTimeoutMs", cfg.writeTimeoutMs},
            {"pingBeforeUse", cfg.pingBeforeUse},
            {"maxConnLifetimeMs", cfg.maxConnLifetimeMs},
            {"maxConnIdleMs", cfg.maxConnIdleMs},
            {"readOnly", cfg.readOnly},
        };
    }

    SqlConfig loadSqlConfigFromFileOrThrow(const std::filesystem::path &filePath)
    {
        return reactor::utils::config::loadConfigAndValidateOrThrow<SqlConfig>(filePath);
    }
} // namespace reactor::persistence
