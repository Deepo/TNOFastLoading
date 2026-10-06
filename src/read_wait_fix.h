#pragma once

#include <windows.h>

#include <cstdint>

#include "patch_result.h"

namespace ReadWaitFix
{
    using Result = Patch::Result;  // the type every module's Apply returns
    using Patch::ToString;

    // exe: WolfNewOrder_x64.exe in memory. Checks that it is the analysed build, then retargets
    // one call (read_wait_fix.cpp): to the exe's SwitchToThread thunk, or with smartWait to the
    // plugin's own wait through a jump near the exe (it waits for the whole read: yields for up
    // to 2 ms, then Sleep(1)). Anything unexpected: writes nothing and says why in the log.
    // yieldUs: SmartWait's yielding time per read (2000; 0 = sleep at once, for testing).
    Result Apply(HMODULE exe, bool smartWait, int yieldUs = 2000);

    // SmartWait's counts since the last call, over all threads: the reads waited for, how many of
    // them ended with the read done (the others: StreamControl shutting down), how many waited
    // past the yielding time and slept, and the longest wait. Each counter is taken on its own, so
    // a read that ends during the call may count in the next set.
    struct WaitStats
    {
        std::uint64_t waits = 0;
        std::uint64_t seenDone = 0;  // waits that ended with the read's flag set
        std::uint64_t slept = 0;
        double longestMs = 0;
    };
    WaitStats TakeStats();

    // The wait for one read: SmartWait's body without the game, so the harness can drive it with
    // a fake clock. Until *done or *shutdown is set (the two things the game's loop checks), it
    // yields while less than `yieldTicks` have passed since the start, then sleeps.
    struct WaitOps
    {
        std::int64_t (*now)();
        void (*yield)();
        void (*sleep)();
    };
    struct WaitResult
    {
        bool done = false;   // ended with the read's flag set (else: StreamControl shutting down)
        bool slept = false;  // waited past the yielding time
        std::int64_t ticks = 0;
    };
    WaitResult WaitForRead(const volatile std::uint8_t* done, const volatile std::uint8_t* shutdown,
                           std::int64_t yieldTicks, const WaitOps& ops);

    // SmartWait's address, so the harness can follow the jump to it.
    const void* SmartWaitForTest();
}
