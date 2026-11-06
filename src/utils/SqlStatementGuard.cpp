#include "utils/SqlStatementGuard.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace reactor::utils
{
    namespace
    {
        std::string_view trim(std::string_view text)
        {
            while (!text.empty() &&
                   std::isspace(static_cast<unsigned char>(text.front())) != 0)
            {
                text.remove_prefix(1);
            }
            while (!text.empty() &&
                   std::isspace(static_cast<unsigned char>(text.back())) != 0)
            {
                text.remove_suffix(1);
            }
            return text;
        }
    } // namespace

    bool isReadOnlyStatement(std::string_view sql)
    {
        sql = trim(sql);
        if (sql.empty())
        {
            return false;
        }

        // 允许一个结尾分号，但拒绝多语句。
        if (sql.back() == ';')
        {
            sql.remove_suffix(1);
            sql = trim(sql);
            if (sql.empty())
            {
                return false;
            }
        }
        if (sql.find(';') != std::string_view::npos)
        {
            return false;
        }

        // 注释可以隐藏首个关键字，直接拒绝。
        if (sql.rfind("--", 0) == 0 || sql.rfind("#", 0) == 0 || sql.rfind("/*", 0) == 0)
        {
            return false;
        }

        size_t keywordLength = 0;
        while (keywordLength < sql.size() &&
               std::isalpha(static_cast<unsigned char>(sql[keywordLength])) != 0)
        {
            ++keywordLength;
        }
        if (keywordLength == 0)
        {
            return false;
        }

        std::string keyword(sql.substr(0, keywordLength));
        std::transform(keyword.begin(), keyword.end(), keyword.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

        return keyword == "SELECT" || keyword == "SHOW" || keyword == "DESCRIBE" ||
               keyword == "DESC" || keyword == "EXPLAIN";
    }
} // namespace reactor::utils
