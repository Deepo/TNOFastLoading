#pragma once

#include <charconv>
#include <filesystem>
#include <string>

#include <ini.h>

// The ini framework both plugins share. Each plugin keeps its own switches in settings.h in its
// folder (src/loadfix, src/tobfix) and has its own Config::Init, which calls Read and then
// ParseConfig for each of its keys.
namespace Config
{
    inline mINI::INIStructure ini;

    std::string ToLower(std::string s);

    // Reads <folder>\<FixName>.ini into `ini`. A missing file is a note in the log: every switch
    // has a default.
    void Read(const std::filesystem::path& folder);

    template <typename T>
    inline T ParseConfig(const std::string& section, const std::string& key, T defaultValue)
    {
        if (!ini.has(section) || !ini[section].has(key)) {
            LOG_INFO("Config: [{}] {} not set, using default: {}", section, key, defaultValue);
            return defaultValue;
        }

        std::string raw = ini[section][key];
        T value;

        if constexpr (std::is_same_v<T, bool>) {
            raw = ToLower(std::move(raw));
            if (raw == "true" || raw == "1") value = true;
            else if (raw == "false" || raw == "0") value = false;
            else {
                LOG_WARN("Config: [{}] {}: \"{}\" is not true/false, using default: {}", section, key, raw, defaultValue);
                return defaultValue;
            }
        }
        else if constexpr (std::is_same_v<T, int>) {
            // The whole value must be the number: "2.5" or "2000oops" is not.
            const auto [end, ec] = std::from_chars(raw.data(), raw.data() + raw.size(), value);
            if (ec != std::errc{} || end != raw.data() + raw.size()) {
                LOG_WARN("Config: [{}] {}: \"{}\" is not a valid value, using default: {}", section, key, raw, defaultValue);
                return defaultValue;
            }
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            value = raw;
        }
        else { static_assert(false, "ParseConfig: unsupported type"); }

        LOG_INFO("Config: [{}] {} = {}", section, key, value);
        return value;
    }

    // The plugin's own: Read, then its keys. Missing file or key: the default, said in the log.
    void Init(const std::filesystem::path& folder);
}
