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

    // exe: WolfNewOrder_x64.exe in memory. Checks that it is the analysed build, then hooks the
    // load screen's wait-for-continue loop. Anything unexpected: no hook, and the log says why.
    // The hook stays for the life of the process.
    Result Install(HMODULE exe);
}
