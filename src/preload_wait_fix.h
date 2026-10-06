#pragma once

#include <windows.h>

#include "patch_result.h"  // Result, ToString

namespace PreloadWaitFix
{
    using Result = Patch::Result;

    // exe: WolfNewOrder_x64.exe in memory. Checks that it is the analysed build, then shortens the
    // material-preload poll from Sleep(100) to Sleep(1) (preload_wait_fix.cpp). Anything
    // unexpected: writes nothing and says why in the log.
    Result Apply(HMODULE exe);
}
