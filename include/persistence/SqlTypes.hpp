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
    enum class SqlErrc : uint8_t
    {
        kOk,
        kConnection,
        kQuery,
        kTimeout,
        kCancelled
    };

    struct SqlError
    {
        SqlErrc code{SqlErrc::kOk};
        int vendorCode{0};
        std::string message;
    };

    using SqlValue = std::variant<std::nullptr_t, uint64_t, int64_t, double, std::string>;
    using SqlRow = std::vector<SqlValue>;
    using SqlRows = std::vector<SqlRow>;

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
