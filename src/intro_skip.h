#pragma once

#include <windows.h>

#include "patch_result.h"  // Result, ToString

namespace IntroSkip
{
    using Result = Patch::Result;

    // exe: WolfNewOrder_x64.exe in memory. Checks that it is the analysed build and that the cvar
    // com_skipIntroVideo does not exist yet, then makes it start as 1 instead of 0, as if the game
    // had been launched with +com_skipIntroVideo 1 (intro_skip.cpp). Anything unexpected: writes
    // nothing and says why in the log.
    Result Apply(HMODULE exe);
}
