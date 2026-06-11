#pragma once

#include <cstdint>

namespace reactor::core
{

/// @brief 通用状态码，统一各模块返回值语义
///
/// 所有返回 StatusCode 的函数均遵循以下约定：
///   - kOk:      操作成功
///   - kAgain:   可重试，调用方应继续循环而非退出
///   - kInvalid: 参数无效（如 fd < 0）
///   - kNotFound: 未找到目标（如 channelMap 中无此 fd）
///   - kError:   通用错误
enum class StatusCode : int32_t
{
    kOk = 0,
    kAgain = 1,
    kInvalid = -2,
    kNotFound = -3,
    kError = -1
};

/// @brief 将 StatusCode 转换为 int32_t，便于日志输出
inline int32_t toInt(StatusCode code)
{
    return static_cast<int32_t>(code);
}
} // namespace reactor::core
