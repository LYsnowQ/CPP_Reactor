#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace reactor::utils::config
{
    // 读取配置文件并解析 JSON，失败时抛出异常。
    nlohmann::json loadJsonFileOrThrow(const std::filesystem::path &filePath);

    template <typename T> T loadConfigOrThrow(const std::filesystem::path &filePath)
    {
        const auto j = loadJsonFileOrThrow(filePath);
        return j.get<T>();
    }

    template <typename T> T loadConfigAndValidateOrThrow(const std::filesystem::path &filePath)
    {
        auto cfg = loadConfigOrThrow<T>(filePath);
        std::string err;
        if (!cfg.validate(err))
        {
            throw std::runtime_error("配置校验失败: " + err);
        }
        return cfg;
    }
} // namespace reactor::utils::config
