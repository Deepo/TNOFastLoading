#include "stdafx.h"
#include "version.h"
#include "logger.h"

#include <cstdio>

namespace Logger
{
    static std::ofstream logFile;
    static bool toConsole = false;
    static std::mutex logMutex;

    void Log(std::string_view level, std::string_view msg)
    {
        std::lock_guard lock(logMutex);
        SYSTEMTIME st;
        GetLocalTime(&st);
        const std::string line = std::format("[{:02d}:{:02d}:{:02d}.{:03d}] [{:5}] [{}] {}\n", st.wHour, st.wMinute,
                                             st.wSecond, st.wMilliseconds, GetCurrentThreadId(), level, msg);
        if (toConsole) {
            std::fputs(line.c_str(), stdout);
            return;
        }
        if (!logFile.is_open()) return;
        // A run writes a few dozen lines at start-up and a few per load screen; no size limit
        // needed.
        logFile << line;
        // Flushed per line: if the game crashes, the last lines are the ones that matter.
        logFile.flush();
    }

    void Init(const std::filesystem::path& folder)
    {
        const std::filesystem::path logPath = folder / (FixName + ".log");
        // Keep one previous generation, so relaunching to check something does not destroy the
        // run that showed the problem.
        std::error_code ec;
        if (std::filesystem::exists(logPath, ec)) {
            const std::filesystem::path prevPath = folder / (FixName + ".prev.log");
            std::filesystem::remove(prevPath, ec);
            std::filesystem::rename(logPath, prevPath, ec);
        }
        logFile.open(logPath, std::ios::trunc);
    }

    void InitConsole()
    {
        toConsole = true;
    }

    std::string Utf8(const std::filesystem::path& p)
    {
        // WideCharToMultiByte without WC_ERR_INVALID_CHARS: a broken UTF-16 name becomes U+FFFD
        // instead of an error.
        const std::wstring& w = p.native();
        if (w.empty()) return {};
        const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (n <= 0) return {};
        std::string s(static_cast<std::size_t>(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
        return s;
    }
}
