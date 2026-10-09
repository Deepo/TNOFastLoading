#pragma once

#include <windows.h>

#include <cstdint>

namespace LoadPrompt
{
    enum class Result { Installed, UnknownBuild, Failed };
    const char* ToString(Result r);

    // What the hook does with one state of the load screen. Pure, so the offline harness can
    // check every case: flags = the render system's load-screen flag word, videoState = the
    // loading video's state (load_prompt.cpp).
    enum class Decision { Continue, Loading, NotTakingInput, VideoPlaying, AlreadyLeaving };
    Decision Decide(std::uint32_t flags, int videoState);
    const char* ToString(Decision d);

    // exe: the game's exe in memory (WolfNewOrder_x64.exe or WolfOldBlood_x64.exe). Checks that it
    // is one of the analysed builds, then hooks the load screen's wait-for-continue loop. Anything
    // unexpected: no hook, and the log says why. The hook stays for the life of the process.
    Result Install(HMODULE exe);
}
