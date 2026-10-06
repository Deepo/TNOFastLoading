#pragma once

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace Logger
{
    // The plugin's log: <folder>\<FixName>.log, keeping the previous run as .prev.log.
    void Init(const std::filesystem::path& folder);
    // The offline harness: the same lines, to stdout.
    void InitConsole();
    void Log(std::string_view level, std::string_view msg);
    // A path for the log, as UTF-8 whatever the system's code page (path.string() throws for
    // characters outside it). Never throws but bad_alloc.
    std::string Utf8(const std::filesystem::path& p);

    // Formatting happens inside the try: a lost log line beats an exception inside the game's
    // process (this runs in DllMain).
    template <typename... Args>
    void Write(std::string_view level, std::format_string<Args...> fmt, Args&&... args) noexcept
    {
        try {
            Log(level, std::format(fmt, std::forward<Args>(args)...));
        }
        catch (...) {
        }
    }
}

#define LOG_INFO(fmt, ...)  Logger::Write("Info",  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::Write("Warn",  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::Write("Error", fmt, ##__VA_ARGS__)
