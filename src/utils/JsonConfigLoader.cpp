#include "utils/JsonConfigLoader.hpp"

#include <fstream>
#include <sstream>

namespace reactor::utils::config
{
    // ====================================================================
    // JSON 配置加载
    // ====================================================================
    //
    // 三步合法性检查，每步失败均抛出 std::runtime_error：
    //   1. ifstream 打开文件失败 → 文件不存在或权限不足
    //   2. 读取内容为空 → 空文件
    //   3. nlohmann::json::parse 失败 → JSON 语法错误
    //
    // 异常信息包含文件路径，便于定位问题配置。
    nlohmann::json loadJsonFileOrThrow(const std::filesystem::path &filePath)
    {
        std::ifstream ifs(filePath);
        if (!ifs.is_open())
        {
            throw std::runtime_error("无法打开配置文件: " + filePath.string());
        }

        std::ostringstream oss;
        oss << ifs.rdbuf();
        const auto raw = oss.str();
        // 空文件检查：即使内容是空的 JSON 对象 "{}"，raw 的长度也大于 0。
        // raw.empty() 仅用于捕获完全空白文件。
        if (raw.empty())
        {
            throw std::runtime_error("配置文件为空: " + filePath.string());
        }

        try
        {
            return nlohmann::json::parse(raw);
        }
        // 捕获 nlohmann 的解析异常并包装为 runtime_error + 文件路径信息。
        catch (const std::exception &ex)
        {
            throw std::runtime_error("JSON 解析失败: " + filePath.string() + ", " + ex.what());
        }
    }
} // namespace reactor::utils::config
