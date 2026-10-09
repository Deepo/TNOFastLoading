#include "stdafx.h"
#include "settings.h"

namespace Config
{
    void Init(const std::filesystem::path& folder)
    {
        Read(folder);
        FastReads          = ParseConfig("Loading", "FastReads", FastReads);
        SmartWait          = ParseConfig("Loading", "SmartWait", SmartWait);
        SmartWaitYieldUs   = ParseConfig("Loading", "SmartWaitYieldUs", SmartWaitYieldUs);
        FastPreloadWait    = ParseConfig("Loading", "FastPreloadWait", FastPreloadWait);
        AutoContinue       = ParseConfig("LoadScreen", "AutoContinue", AutoContinue);
        SkipIntroVideo     = ParseConfig("Startup", "SkipIntroVideo", SkipIntroVideo);
        SkipWarningScreens = ParseConfig("Startup", "SkipWarningScreens", SkipWarningScreens);
        FastQuit           = ParseConfig("Quit", "FastQuit", FastQuit);
    }
}
