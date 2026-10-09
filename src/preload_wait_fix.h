#pragma once

#include <windows.h>

#include "patch_result.h"  // Result, ToString

namespace PreloadWaitFix
{
    using Result = Patch::Result;

    // exe: the game's exe in memory (WolfNewOrder_x64.exe or WolfOldBlood_x64.exe). Checks that it
    // is one of the analysed builds, then shortens the material-preload poll from Sleep(100) to
    // Sleep(1) (preload_wait_fix.cpp). Anything unexpected: writes nothing and says why in the log.
    Result Apply(HMODULE exe);
}
