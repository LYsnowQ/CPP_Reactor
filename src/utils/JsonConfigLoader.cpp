#include "utils/JsonConfigLoader.hpp"

#include <fstream>
#include <sstream>

namespace reactor::utils::config
{
nlohmann::json loadJsonFileOrThrow(const std::filesystem::path& filePath)
{
    std::ifstream ifs(filePath);
    if(!ifs.is_open())
    {
        throw std::runtime_error("无法打开配置文件: " + filePath.string());
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    const auto raw = oss.str();
    if(raw.empty())
    {
        throw std::runtime_error("配置文件为空: " + filePath.string());
    }

    try
    {
        return nlohmann::json::parse(raw);
    }
    catch(const std::exception& ex)
    {
        throw std::runtime_error("JSON 解析失败: " + filePath.string() + ", " + ex.what());
    }
}
} // namespace reactor::utils::config
