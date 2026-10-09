#pragma once

#include <windows.h>

#include "patch_result.h"  // Result, ToString

namespace IntroSkip
{
    using Result = Patch::Result;

    // exe: the game's exe in memory (WolfNewOrder_x64.exe or WolfOldBlood_x64.exe). Checks that it
    // is one of the analysed builds and that the cvar com_skipIntroVideo does not exist yet, then
    // makes it start as 1 instead of 0, as if the game had been launched with +com_skipIntroVideo 1
    // (intro_skip.cpp). Anything unexpected: writes nothing and says why in the log.
    Result Apply(HMODULE exe);

    // The Old Blood only: the same for its cvar skipInitialWarningScreens, so the warning screens
    // before the main menu are skipped (intro_skip.cpp). Any other build: UnknownBuild.
    Result ApplyWarningScreens(HMODULE exe);
}
