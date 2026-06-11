#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace reactor::utils
{

/// @brief URL 编解码工具函数集
///
/// 提供 URL 解码（urlDecode）、路径组件按段编码（urlEncodePathComponent）
/// 以及十六进制字符转换（hexValue）三个 inline 函数。
///
/// 所有函数为内联实现，零依赖。
/// @thread safe（纯函数，无状态）

/// @brief 将十六进制字符转换为数值
/// @param c 十六进制字符（'0'-'9', 'a'-'f', 'A'-'F'）
/// @return 对应的数值（0-15），非法字符返回 -1
inline int hexValue(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

/// @brief URL 解码（Percent-decoding）
///
/// 将 %XX 形式的编码序列还原为原始字符，'+' 转换为空格。
/// 非编码字符原样保留。
///
/// @param in 编码后的字符串（如 "hello%20world%21"）
/// @return 解码后的字符串（如 "hello world!"）
/// @thread safe
inline std::string urlDecode(std::string_view in)
{
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i)
    {
        const char c = in[i];
        if (c == '%' && i + 2 < in.size())
        {
            const int hi = hexValue(in[i + 1]);
            const int lo = hexValue(in[i + 2]);
            if (hi >= 0 && lo >= 0)
            {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        if (c == '+')
        {
            out.push_back(' ');
            continue;
        }
        out.push_back(c);
    }
    return out;
}

/// @brief URL 路径组件按段编码
///
/// 将非 ASCII 字符和 URL 保留字符（除 - _ . ~ 外）编码为 %XX 格式。
/// 保留字符集：大小写字母、数字、- _ . ~ 原样保留。
///
/// 与 urlDecode 配合使用，确保 static file handler 中的
/// 目录/文件名包含中文时路径正确。
///
/// @param in 原始字符串
/// @return 编码后的字符串
/// @thread safe
inline std::string urlEncodePathComponent(std::string_view in)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : in)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
        {
            out.push_back(static_cast<char>(c));
        }
        else
        {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0x0F]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}
} // namespace reactor::utils
