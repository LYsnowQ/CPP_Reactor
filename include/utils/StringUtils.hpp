#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace reactor::utils
{
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
