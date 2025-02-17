#pragma once

#include "bmcweb_config.h"

#include <boost/system/error_code.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/url/string_view.hpp>
#include <boost/url/url.hpp>
#include <nlohmann/json.hpp>

#include <bit>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>
#include <system_error>

// Clang-tidy would rather these be static, but using static causes the template
// specialization to not function.  Ignore the warning.
// NOLINTBEGIN(readability-convert-member-functions-to-static, cert-dcl58-cpp)
template <>
struct std::formatter<boost::system::error_code>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(const boost::system::error_code& ec, auto& ctx) const
    {
        return std::format_to(ctx.out(), "{}", ec.what());
    }
};

template <>
struct std::formatter<boost::urls::pct_string_view>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(const boost::urls::pct_string_view& msg, auto& ctx) const
    {
        return std::format_to(ctx.out(), "{}",
                              std::string_view(msg.data(), msg.size()));
    }
};

template <>
struct std::formatter<boost::urls::url>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(const boost::urls::url& msg, auto& ctx) const
    {
        return std::format_to(ctx.out(), "{}", std::string_view(msg.buffer()));
    }
};

template <>
struct std::formatter<boost::core::string_view>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(const boost::core::string_view& msg, auto& ctx) const
    {
        return std::format_to(ctx.out(), "{}", std::string_view(msg));
    }
};

template <>
struct std::formatter<void*>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(const void*& ptr, auto& ctx) const
    {
        return std::format_to(ctx.out(), "{}",
                              std::to_string(std::bit_cast<size_t>(ptr)));
    }
};

template <>
struct std::formatter<nlohmann::json::json_pointer>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(const nlohmann::json::json_pointer& ptr, auto& ctx) const
    {
        return std::format_to(ctx.out(), "{}", ptr.to_string());
    }
};

template <>
struct std::formatter<nlohmann::json>
{
    static constexpr auto parse(std::format_parse_context& ctx)
    {
        return ctx.begin();
    }
    auto format(const nlohmann::json& json, auto& ctx) const
    {
        return std::format_to(
            ctx.out(), "{}",
            json.dump(-1, ' ', false,
                      nlohmann::json::error_handler_t::replace));
    }
};
// NOLINTEND(readability-convert-member-functions-to-static, cert-dcl58-cpp)

namespace crow
{
enum class LogLevel
{
    Disabled = 0,
    Critical,
    Error,
    Warning,
    Info,
    Debug,
    Enabled,
};

// Mapping of the external loglvl name to internal loglvl
constexpr std::array<std::string_view, 7> mapLogLevelFromName{
    "DISABLED", "CRITICAL", "ERROR", "WARNING", "INFO", "DEBUG", "ENABLED"};

constexpr crow::LogLevel getLogLevelFromName(std::string_view name)
{
    const auto* iter = std::ranges::find(mapLogLevelFromName, name);
    if (iter != mapLogLevelFromName.end())
    {
        return static_cast<LogLevel>(iter - mapLogLevelFromName.begin());
    }
    return crow::LogLevel::Disabled;
}

// configured bmcweb LogLevel
constexpr crow::LogLevel bmcwebCurrentLoggingLevel =
    getLogLevelFromName(bmcwebLoggingLevel);

struct FormatString
{
    std::string_view str;
    std::source_location loc;

    // NOLINTNEXTLINE(google-explicit-constructor)
    FormatString(const char* strIn, const std::source_location& locIn =
                                        std::source_location::current()) :
        str(strIn),
        loc(locIn)
    {}
};

template <typename T>
const void* logPtr(T p)
{
    static_assert(std::is_pointer<T>::value,
                  "Can't use logPtr without pointer");
    return std::bit_cast<const void*>(p);
}

template <LogLevel level>
inline void vlog(const FormatString& format, std::format_args&& args)
{
    if constexpr (bmcwebCurrentLoggingLevel < level)
    {
        return;
    }
    constexpr size_t stringIndex = static_cast<size_t>(level);
    static_assert(stringIndex < mapLogLevelFromName.size(),
                  "Missing string for level");
    constexpr std::string_view levelString = mapLogLevelFromName[stringIndex];
    std::string_view filename = format.loc.file_name();
    filename = filename.substr(filename.rfind('/') + 1);
    std::cout << std::format("[{} {}:{}] ", levelString, filename,
                             format.loc.line())
              << std::vformat(format.str, args) << '\n';
}
} // namespace crow

template <typename... Args>
inline void BMCWEB_LOG_CRITICAL(const crow::FormatString& format,
                                Args&&... args)
{
    crow::vlog<crow::LogLevel::Critical>(
        format, std::make_format_args(std::forward<Args>(args)...));
}

template <typename... Args>
inline void BMCWEB_LOG_ERROR(const crow::FormatString& format, Args&&... args)
{
    crow::vlog<crow::LogLevel::Error>(
        format, std::make_format_args(std::forward<Args>(args)...));
}

template <typename... Args>
inline void BMCWEB_LOG_WARNING(const crow::FormatString& format, Args&&... args)
{
    crow::vlog<crow::LogLevel::Warning>(
        format, std::make_format_args(std::forward<Args>(args)...));
}

template <typename... Args>
inline void BMCWEB_LOG_INFO(const crow::FormatString& format, Args&&... args)
{
    crow::vlog<crow::LogLevel::Info>(
        format, std::make_format_args(std::forward<Args>(args)...));
}

template <typename... Args>
inline void BMCWEB_LOG_DEBUG(const crow::FormatString& format, Args&&... args)
{
    crow::vlog<crow::LogLevel::Debug>(
        format, std::make_format_args(std::forward<Args>(args)...));
}
