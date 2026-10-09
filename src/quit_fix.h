#pragma once

#include <windows.h>

#include "patch_result.h"  // Result, ToString

namespace QuitFix
{
    using Result = Patch::Result;

    // The Old Blood only. exe: WolfOldBlood_x64.exe in memory. Checks that it is one of the analysed
    // builds, then removes the call that blanks the virtual-texture page caches page by page at exit,
    // just before they are deleted (quit_fix.cpp). Any other build, or anything unexpected: writes
    // nothing and says why in the log.
    Result Apply(HMODULE exe);
}
