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

        // 注释可以隐藏关键字（如 INTO/**/OUTFILE），直接拒绝任何注释序列。
        if (sql.find("--") != std::string_view::npos ||
            sql.find('#') != std::string_view::npos ||
            sql.find("/*") != std::string_view::npos)
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

        if (keyword != "SELECT" && keyword != "SHOW" && keyword != "DESCRIBE" &&
            keyword != "DESC" && keyword != "EXPLAIN")
        {
            return false;
        }

        // 去掉空白后做大小写不敏感的危险模式匹配，避免 INTO\nOUTFILE、
        // LOAD_FILE ( ... ) 这类写法绕过。
        std::string compact;
        compact.reserve(sql.size());
        for (char c : sql)
        {
            const auto uc = static_cast<unsigned char>(c);
            if (std::isspace(uc) != 0)
            {
                continue;
            }
            compact.push_back(static_cast<char>(std::toupper(uc)));
        }

        static constexpr std::string_view kForbiddenTokens[] = {
            "INTOOUTFILE",     // 写服务器文件
            "INTODUMPFILE",    // 写服务器文件
            "FORUPDATE",       // 行锁
            "FORSHARE",        // 行锁（MySQL 8 的 LOCK IN SHARE MODE 同义）
            "LOCKINSHAREMODE", // 行锁
            "LOAD_FILE(",       // 读服务器文件
            "SLEEP(",          // 占用连接
            "BENCHMARK(",      // 占用 CPU/连接
            "GET_LOCK(",       // 命名锁
            "RELEASE_LOCK(",   // 命名锁
        };

        for (const auto token : kForbiddenTokens)
        {
            if (compact.find(token) != std::string::npos)
            {
                return false;
            }
        }

        return true;
    }
} // namespace reactor::utils
