#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace reactor::utils::config
{

/// @brief 读取文件并解析为 JSON 对象
///
/// 完整校验流程：
///   1. 文件是否存在/可打开
///   2. 文件内容是否为空
///   3. JSON 语法是否合法
///
/// @param filePath 配置文件路径
/// @return 解析后的 JSON 对象
/// @throw std::runtime_error 文件不存在/为空/JSON 解析失败
nlohmann::json loadJsonFileOrThrow(const std::filesystem::path &filePath);

/// @brief 读取 JSON 文件并转换为指定类型
///
/// 在 loadJsonFileOrThrow 基础上调用 j.get<T>() 进行类型转换。
///
/// @tparam T 目标类型（需满足 nlohmann::json::get 要求）
/// @param filePath 配置文件路径
/// @return 转换后的类型实例
/// @throw std::runtime_error 文件读取失败
/// @throw nlohmann::json::exception JSON 类型不匹配
template <typename T>
T loadConfigOrThrow(const std::filesystem::path &filePath)
{
    const auto j = loadJsonFileOrThrow(filePath);
    return j.get<T>();
}

/// @brief 读取 JSON 文件、转换并执行 validate 校验
///
/// 在 loadConfigOrThrow 基础上调用 cfg.validate(err)。
/// 适用于配置类定义了 validate(std::string&) 方法的场景。
///
/// @tparam T 目标类型（需满足 j.get<T>() + T::validate 要求）
/// @param filePath 配置文件路径
/// @return 校验通过后的类型实例
/// @throw std::runtime_error 文件读取失败/JSON 解析失败/校验不通过
template <typename T>
T loadConfigAndValidateOrThrow(const std::filesystem::path &filePath)
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
