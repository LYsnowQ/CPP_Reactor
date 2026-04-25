#pragma once

#include <cstdint>

namespace reactor::core
{
    enum class StatusCode : int32_t
    {
        kOk = 0,
        kAgain = 1,
        kInvalid = -2,
        kNotFound = -3,
        kError = -1
    };

    inline int32_t toInt(StatusCode code)
    {
        return static_cast<int32_t>(code);
    }
}

