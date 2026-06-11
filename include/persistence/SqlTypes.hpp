#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <string_view>
#include <memory>
#include <chrono>
#include <variant>
#include <optional>

namespace reactor::persistence
{

/// @brief SQL 操作错误码
///
/// @param kOk          操作成功
/// @param kConnection  连接相关错误（断开、超时、认证）
/// @param kQuery       SQL 查询执行错误
/// @param kTimeout     操作超时
/// @param kCancelled   操作被取消
enum class SqlErrc : uint8_t
{
    kOk,
    kConnection,
    kQuery,
    kTimeout,
    kCancelled
};

/// @brief SQL 错误描述
///
/// @param code       错误码
/// @param vendorCode 数据库厂商错误码（如 MySQL 的 1062）
/// @param message    可读错误描述
struct SqlError
{
    SqlErrc code{SqlErrc::kOk};
    int vendorCode{0};
    std::string message;
};

/// @brief SQL 值类型（variant，支持 NULL 和常用数值/字符串类型）
///
/// 支持类型：nullptr_t, uint64_t, int64_t, double, std::string
using SqlValue = std::variant<std::nullptr_t, uint64_t, int64_t, double, std::string>;

/// @brief 一行数据（SqlValue 向量）
using SqlRow = std::vector<SqlValue>;

/// @brief 结果集（SqlRow 向量）
using SqlRows = std::vector<SqlRow>;

/// @brief 带错误信息的泛型结果包装
///
/// @tparam T 值类型
/// @param ok    操作是否成功
/// @param value 成功时的值（ok=true 时有效）
/// @param err   错误描述（ok=false 时有效）
///
/// 特化 Result<void> 不含 value 字段。
template <class T> struct Result
{
    bool ok{false};
    std::optional<T> value;
    SqlError err{};
};

template<> struct Result<void>
{
    bool ok{false};
    SqlError err{};
};
} // namespace reactor::persistence
