#include "stdafx.h"
#include "version.h"
#include "settings.h"
#include "read_wait_fix.h"
#include "preload_wait_fix.h"
#include "load_prompt.h"
#include "intro_skip.h"

// ---------------------------------------------------------------------------
// TNOFastLoading ("Wolfenstein The New Order - Fast Loading") - shorter loads in Wolfenstein: The New
// Order (the GOG, Steam, Epic and Game Pass builds; read_wait_fix.cpp).
//
// 1. FastReads (read_wait_fix.cpp): the game waits for each synchronous stream read by polling
//    with Sleep(1), about 2 ms per read however fast the disk is, thousands of times per load.
//    The poll yields instead. SmartWait (the default) waits for each
//    read itself, yielding for up to 2 ms and then sleeping, so a slow hard drive doesn't keep a
//    core busy.
// 2. FastPreloadWait (preload_wait_fix.cpp): after each map load the game polls a material
//    preload with Sleep(100): a fixed 100 ms per load. 1 byte: it polls every 1 ms.
// 3. AutoContinue (load_prompt.cpp): after the load, the game waits for "press X". A hook in that
//    wait loop does what the press does, unless a loading cutscene plays.
// 4. SkipIntroVideo (intro_skip.cpp): com_skipIntroVideo starts as 1, so the start-up logo video
//    doesn't hold the game ~30 s.
//
// Loaded by Ultimate ASI Loader as winmm.dll (the exe imports WINMM), which loads *.asi from
// the game folder at start-up, before the game's own start-up code. Each part is switchable in
// TNOFastLoading.ini next to the plugin; all of them are set up once, here in DllMain. Afterwards
// only SmartWait (on the reading threads) and the hook (on the main thread while a load screen
// waits) run. Files: the ini (read) and the log, <game>\TNOFastLoading.log (previous run:
// .prev.log). Any other build of the game is left untouched; the log says which one it saw.
// ---------------------------------------------------------------------------

// One part of the set-up. An exception (bad_alloc is the only one expected) ends just that part,
// and the log says so; the parts after it still run.
template <typename F>
static void Step(const char* part, F&& run)
{
    try {
        run();
    }
    catch (const std::exception& e) {
        LOG_ERROR("{}: stopped by an error ({}); the lines above show how far it got.", part, e.what());
    }
    catch (...) {
        LOG_ERROR("{}: stopped by an error; the lines above show how far it got.", part);
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    // No DisableThreadLibraryCalls: the static C runtime needs the thread notifications.
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    try {
        wchar_t buf[MAX_PATH]{};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        const std::filesystem::path exePath(buf);
        // Ultimate ASI Loader only loads into the game, but stay out of anything else regardless:
        // no log either, so a stray process cannot rotate the real run's log away.
        if (_wcsicmp(exePath.filename().c_str(), L"WolfNewOrder_x64.exe") != 0) return TRUE;

        GetModuleFileNameW(module, buf, MAX_PATH);
        const std::filesystem::path asiPath(buf);
        // If the log can't be written (a protected folder), Log() just drops the lines: every
        // patch below still runs.
        Logger::Init(asiPath.parent_path());

        SYSTEMTIME st;
        GetLocalTime(&st);
        LOG_INFO("----------");
        LOG_INFO("{} v{} (built {} {})", FixName, FixVersion, __DATE__, __TIME__);
        LOG_INFO("Session start: {:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}", st.wYear, st.wMonth, st.wDay, st.wHour,
                 st.wMinute, st.wSecond);
        LOG_INFO("Plugin: {} at {}", Logger::Utf8(asiPath), static_cast<void*>(module));
        LOG_INFO("Host: {} (pid {})", Logger::Utf8(exePath), GetCurrentProcessId());
        // Without the ini every switch keeps its default (on).
        Step("Config", [&] { Config::Init(asiPath.parent_path()); });
        LOG_INFO("----------");

        const HMODULE exe = GetModuleHandleW(nullptr);
        Step("FastReads", [&] {
            if (Config::FastReads)
                LOG_INFO("FastReads: {}", Patch::ToString(ReadWaitFix::Apply(exe, Config::SmartWait, Config::SmartWaitYieldUs)));
            else
                LOG_INFO("FastReads: off in the ini, nothing changed");
        });
        Step("FastPreloadWait", [&] {
            if (Config::FastPreloadWait)
                LOG_INFO("FastPreloadWait: {}", Patch::ToString(PreloadWaitFix::Apply(exe)));
            else
                LOG_INFO("FastPreloadWait: off in the ini, nothing changed");
        });
        Step("AutoContinue", [&] {
            if (Config::AutoContinue)
                LOG_INFO("AutoContinue: {}", LoadPrompt::ToString(LoadPrompt::Install(exe)));
            else
                LOG_INFO("AutoContinue: off in the ini, nothing changed");
        });
        Step("SkipIntroVideo", [&] {
            if (Config::SkipIntroVideo)
                LOG_INFO("SkipIntroVideo: {}", Patch::ToString(IntroSkip::Apply(exe)));
            else
                LOG_INFO("SkipIntroVideo: off in the ini, nothing changed");
        });
        LOG_INFO("----------");
    }
    catch (...) {
        // Only the lines before the first Step can get here (bad_alloc): nothing was changed.
        LOG_ERROR("Start-up stopped by an error: nothing was changed.");
    }
    return TRUE;
}
