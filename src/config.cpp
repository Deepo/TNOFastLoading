#include "stdafx.h"
#include "version.h"
#include "config.h"

#include <algorithm>
#include <cctype>

namespace Config
{
    std::string ToLower(std::string s)
    {
        std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    void Read(const std::filesystem::path& folder)
    {
        const std::filesystem::path path = folder / (FixName + ".ini");
        // mINI opens the file through the path itself (UTF-16 on Windows), so a folder name in
        // any script works; path.string() would throw for characters outside the system's code
        // page. The log gets UTF-8.
        mINI::INIFile file(path);

        // Every switch has a default, so a missing ini is a note in the log, not an error.
        if (file.read(ini))
            LOG_INFO("Config file: {}", Logger::Utf8(path));
        else
            LOG_WARN("Config file {} not found or not readable, using defaults", Logger::Utf8(path));
    }
}
