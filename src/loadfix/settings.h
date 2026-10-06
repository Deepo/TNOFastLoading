#pragma once

#include "config.h"

// TNOFastLoading's switches, read from TNOFastLoading.ini (settings.cpp).
namespace Config
{
    // [Loading] FastReads: the main fix (read_wait_fix.cpp). Stream reads yield instead of
    // sleeping 1 ms per read: a level load ~10 s -> ~2 s.
    inline bool FastReads = true;

    // [Loading] SmartWait: with FastReads, each read yields for up to 2 ms and then sleeps 1 ms at a
    // time, so a slow hard drive doesn't keep a core busy during loads; false: always yield (the
    // plain 4-byte FastReads). The same speed on SSDs.
    inline bool SmartWait = true;

    // [Loading] SmartWaitYieldUs: SmartWait's yielding time per read in microseconds (2000). Not in the
    // shipped ini: 0 sends every read straight to the sleep path, to test it on a fast disk.
    inline int SmartWaitYieldUs = 2000;

    // [Loading] FastPreloadWait: after each load the game polls the virtual-texture material
    // preload with Sleep(100), so every load waits a full 100 ms for work that takes ~0 ms
    // (preload_wait_fix.cpp). Polls every 1 ms instead.
    inline bool FastPreloadWait = true;

    // [LoadScreen] AutoContinue: go into the game as soon as loading is done, as if the
    // continue button had been pressed (load_prompt.cpp). Never while a loading cutscene plays.
    inline bool AutoContinue = true;

    // [Startup] SkipIntroVideo: no logo video at start-up, as with the launch option
    // +com_skipIntroVideo 1 (intro_skip.cpp). The video cannot be skipped and holds the game
    // ~30 s after start-up loading is done.
    inline bool SkipIntroVideo = true;
}
