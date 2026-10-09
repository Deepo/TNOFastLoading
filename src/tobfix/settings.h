#pragma once

#include "config.h"

// TOBFastLoading's switches, read from TOBFastLoading.ini (settings.cpp). TNOFastLoading's switches
// plus SkipWarningScreens and FastQuit.
namespace Config
{
    // [Loading] FastReads: the main fix (read_wait_fix.cpp). Stream reads yield instead of
    // sleeping 1 ms per read.
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

    // [Startup] SkipIntroVideo: no logo video at start-up (~30 s): com_skipIntroVideo starts as 1, as
    // with the launch option +com_skipIntroVideo 1, and Init2 then plays no video at all, as in The
    // New Order (by itself The Old Blood would still show the logo while start-up loading runs;
    // intro_skip.cpp).
    inline bool SkipIntroVideo = true;

    // [Startup] SkipWarningScreens: the game's own cvar skipInitialWarningScreens starts as 1, so the
    // photosensitivity warning and the auto-save notice before the main menu are skipped
    // (intro_skip.cpp).
    inline bool SkipWarningScreens = true;

    // [Quit] FastQuit: at exit the game blanks its virtual-texture page caches page by page just
    // before deleting them: about 1 s at Quit to Desktop, about 10 s with the cache sizes raised in
    // graphicsprofiles.json. The call is removed (quit_fix.cpp).
    inline bool FastQuit = true;
}
