#include "stdafx.h"
#include "load_prompt.h"
#include "read_wait_fix.h"  // SmartWait's counts, logged at each load screen

#include <safetyhook.hpp>

// ---------------------------------------------------------------------------
// The "press X to continue" prompt after a load.
//
// When a level has loaded, the resource manager's LoadMap (RVA 0x16c980) sets the load screen's
// "load done" flag and calls 0x166c50, which waits for the player:
//
//   0x166d95  test dil, dil / abort byte / quit check   ; leave the loop
//   0x166dba  cmp  [skipLoadingVideoAutomatically], 0   ; developer cvar: leave the loop  <- hook
//   0x166dc7  if (renderSystem->HasFlag(8)) read up to 20 input events; a continue key
//             (bound to _use, Enter, Space; or the Flash GUI's _menuaccept) does
//             renderSystem->ClearFlag(7)
//   0x166f0e  session pump, Sleep(10)
//   0x166f38  renderSystem->IsVideoDone()  (flag 1 clear, video state not 3/4) -> leave
//
// During the prompt the flag word is 0x59 (1 load screen up, 8 taking input, 0x10 load done,
// 0x40 no video) and the video state 0. A press turns the flags into 0x58, and the loop leaves at
// its next check.
//
// The hook does what the press does, ClearFlag(7) through the game's own function, and only when
// a press would simply continue: load done, load screen up, the game taking input, and no loading
// video (flag 0x20 clear, video state not 3 or 4). The loop then leaves through its own check, as
// after a press. While a cutscene plays, the hook does nothing: the cutscene plays, the player can
// skip it as usual, and if it ends with the prompt still up, the hook continues then.
// The developer cvar skipLoadingVideoAutomatically also leaves this loop, but it isn't used: it
// would cut the story cutscenes that play while a chapter loads (base\bink\loading\c01..c16) as
// soon as the map is loaded.
//
// The 2021 build (Epic, see read_wait_fix.cpp) has the same loop, its exit checks from 0x1766a7
// and the hook at 0x1766cc (its cmp uses r14d instead of r12d), and the same render system:
// idRenderSystemLocal's vtable has SetFlag, ClearFlag, HasFlag and IsVideoDone in slots 19-22, the
// flags at +0xc, the video at +0x9b0 and its state at +0x60. Only the three flag accessors aren't
// next to each other there. The Game Pass build has the same code at other addresses (the hook at
// 0x176adc).
// ---------------------------------------------------------------------------

#include <span>

namespace LoadPrompt
{
    constexpr std::uint32_t kTimeDateStamp = 0x53A185CF;  // same build as read_wait_fix.cpp

    // The loop's exit checks, from `test dil, dil` to the input branch (0x4f bytes); the hook site
    // is inside: 0x166dba, the 7-byte `cmp dword [rip+0x1b6cad7], r12d`.
    constexpr std::uintptr_t kLoopChecks = 0x166D95;
    constexpr std::uint8_t kLoopChecksBytes[] = {
        0x40, 0x84, 0xFF,                          // test dil, dil
        0x0F, 0x84, 0xB2, 0x01, 0x00, 0x00,        // je   0x166f50 (leave)
        0x0F, 0xB6, 0x86, 0xC0, 0x3F, 0x00, 0x00,  // movzx eax, byte [rsi+0x3fc0] (abort)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x85, 0xA3, 0x01, 0x00, 0x00,        // jne  0x166f50
        0xE8, 0x4E, 0x0C, 0x8D, 0x00,              // call 0xa37a00 (quitting?)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x85, 0x96, 0x01, 0x00, 0x00,        // jne  0x166f50
        0x44, 0x39, 0x25, 0xD7, 0xCA, 0xB6, 0x01,  // cmp  [skipLoadingVideoAutomatically], r12d  <- hook
        0x0F, 0x85, 0x89, 0x01, 0x00, 0x00,        // jne  0x166f50
        0x48, 0x8B, 0x0D, 0xDA, 0xA8, 0x43, 0x01,  // mov  rcx, [renderSystem]
        0xBA, 0x08, 0x00, 0x00, 0x00,              // mov  edx, 8
        0x48, 0x8B, 0x01,                          // mov  rax, [rcx]
        0xFF, 0x90, 0xA8, 0x00, 0x00, 0x00,        // call [rax+0xa8] (HasFlag)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x84, 0x2A, 0x01, 0x00, 0x00,        // je   0x166f0e
    };
    constexpr std::uintptr_t kHookAt = 0x166DBA;
    static_assert(kHookAt == kLoopChecks + 0x25);

    // The loop's tail: Sleep(10), then IsVideoDone() decides whether to leave.
    constexpr std::uintptr_t kLoopTail = 0x166F2E;
    constexpr std::uint8_t kLoopTailBytes[] = {
        0xB9, 0x0A, 0x00, 0x00, 0x00,              // mov  ecx, 10
        0xE8, 0x78, 0x50, 0x87, 0x00,              // call Sys_Sleep
        0x48, 0x8B, 0x0D, 0x69, 0xA7, 0x43, 0x01,  // mov  rcx, [renderSystem]
        0x48, 0x8B, 0x01,                          // mov  rax, [rcx]
        0xFF, 0x90, 0xB0, 0x00, 0x00, 0x00,        // call [rax+0xb0] (IsVideoDone)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x84, 0x45, 0xFE, 0xFF, 0xFF,        // je   0x166d95 (loop)
    };

    // idRenderSystemLocal's flag accessors (the flag word is at +0xc) and IsVideoDone.
    constexpr std::uintptr_t kFlagFns = 0x4608C0;
    constexpr std::uint8_t kFlagFnsBytes[] = {
        0x09, 0x51, 0x0C, 0xC3,                          // SetFlag:   or  [rcx+0xc], edx; ret
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0xF7, 0xD2, 0x21, 0x51, 0x0C, 0xC3,              // ClearFlag: not edx; and [rcx+0xc], edx; ret
        0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
        0x85, 0x51, 0x0C, 0x0F, 0x95, 0xC0, 0xC3,        // HasFlag:   test [rcx+0xc], edx; setne al; ret
    };
    constexpr std::uintptr_t kIsVideoDone = 0x4640E0;
    constexpr std::uint8_t kIsVideoDoneBytes[] = {
        0x48, 0x83, 0xEC, 0x28,                    // sub  rsp, 0x28
        0x48, 0x8B, 0x81, 0xB0, 0x09, 0x00, 0x00,  // mov  rax, [rcx+0x9b0]    (the video)
        0x8B, 0x50, 0x60,                          // mov  edx, [rax+0x60]     (its state)
        0x83, 0xFA, 0x04, 0x74, 0x1E,              // cmp  edx, 4; je  false
        0x83, 0xFA, 0x03, 0x74, 0x19,              // cmp  edx, 3; je  false
        0x48, 0x8B, 0x01,                          // mov  rax, [rcx]
        0xBA, 0x01, 0x00, 0x00, 0x00,              // mov  edx, 1
        0xFF, 0x90, 0xA8, 0x00, 0x00, 0x00,        // call [rax+0xa8] (HasFlag(1))
        0x84, 0xC0, 0x75, 0x07,                    // test al, al; jne false
        0xB0, 0x01, 0x48, 0x83, 0xC4, 0x28, 0xC3,  // true
        0x32, 0xC0, 0x48, 0x83, 0xC4, 0x28, 0xC3,  // false
    };

    constexpr std::uintptr_t kRenderSystemPtr = 0x15A16A8;   // idRenderSystem* renderSystem (= &tr)
    constexpr std::uintptr_t kRenderSystemObj = 0x1D1ADA0;   // tr, idRenderSystemLocal
    constexpr std::uintptr_t kRenderSystemVtbl = 0xEE5B68;   // idRenderSystemLocal's vtable
    constexpr std::size_t kSlotSetFlag = 19, kSlotClearFlag = 20, kSlotHasFlag = 21, kSlotIsVideoDone = 22;
    constexpr std::size_t kFlagsOffset = 0xC;
    constexpr std::size_t kVideoOffset = 0x9B0, kVideoStateOffset = 0x60;

    // The 2021 build (Epic).
    constexpr std::uint32_t kTimeDateStamp2021 = 0x611A423D;
    constexpr std::uintptr_t kLoopChecks2021 = 0x1766A7;
    constexpr std::uint8_t kLoopChecks2021Bytes[] = {
        0x40, 0x84, 0xFF,                          // test dil, dil
        0x0F, 0x84, 0xB0, 0x01, 0x00, 0x00,        // je   0x176860 (leave)
        0x0F, 0xB6, 0x86, 0xC0, 0x3F, 0x00, 0x00,  // movzx eax, byte [rsi+0x3fc0] (abort)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x85, 0xA1, 0x01, 0x00, 0x00,        // jne  0x176860
        0xE8, 0x5C, 0xCF, 0x45, 0x00,              // call 0x5d3620 (quitting?)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x85, 0x94, 0x01, 0x00, 0x00,        // jne  0x176860
        0x44, 0x39, 0x35, 0xE5, 0x0F, 0xA0, 0x01,  // cmp  [skipLoadingVideoAutomatically], r14d  <- hook
        0x0F, 0x85, 0x87, 0x01, 0x00, 0x00,        // jne  0x176860
        0x48, 0x8B, 0x0D, 0x08, 0xC2, 0xFA, 0x00,  // mov  rcx, [renderSystem]
        0xBA, 0x08, 0x00, 0x00, 0x00,              // mov  edx, 8
        0x48, 0x8B, 0x01,                          // mov  rax, [rcx]
        0xFF, 0x90, 0xA8, 0x00, 0x00, 0x00,        // call [rax+0xa8] (HasFlag)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x84, 0x28, 0x01, 0x00, 0x00,        // je   0x17681e
    };
    constexpr std::uintptr_t kHookAt2021 = 0x1766CC;
    static_assert(kHookAt2021 == kLoopChecks2021 + 0x25);
    constexpr std::uintptr_t kLoopTail2021 = 0x17683E;
    constexpr std::uint8_t kLoopTail2021Bytes[] = {
        0xB9, 0x0A, 0x00, 0x00, 0x00,              // mov  ecx, 10
        0xE8, 0xB8, 0x09, 0x8D, 0x00,              // call Sys_Sleep
        0x48, 0x8B, 0x0D, 0x99, 0xC0, 0xFA, 0x00,  // mov  rcx, [renderSystem]
        0x48, 0x8B, 0x01,                          // mov  rax, [rcx]
        0xFF, 0x90, 0xB0, 0x00, 0x00, 0x00,        // call [rax+0xb0] (IsVideoDone)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x84, 0x47, 0xFE, 0xFF, 0xFF,        // je   0x1766a7 (loop)
    };
    constexpr std::uintptr_t kSetFlag2021 = 0x44D1C0;
    constexpr std::uint8_t kSetFlag2021Bytes[] = {0x09, 0x51, 0x0C, 0xC3};                    // or  [rcx+0xc], edx; ret
    constexpr std::uintptr_t kClearFlag2021 = 0x44CE10;
    constexpr std::uint8_t kClearFlag2021Bytes[] = {0xF7, 0xD2, 0x21, 0x51, 0x0C, 0xC3};      // not edx; and [rcx+0xc], edx; ret
    constexpr std::uintptr_t kHasFlag2021 = 0x44B140;
    constexpr std::uint8_t kHasFlag2021Bytes[] = {0x85, 0x51, 0x0C, 0x0F, 0x95, 0xC0, 0xC3};  // test [rcx+0xc], edx; setne al; ret
    constexpr std::uintptr_t kIsVideoDone2021 = 0x44E230;
    constexpr std::uint8_t kIsVideoDone2021Bytes[] = {
        0x48, 0x83, 0xEC, 0x28,                    // sub  rsp, 0x28
        0x48, 0x8B, 0x81, 0xB0, 0x09, 0x00, 0x00,  // mov  rax, [rcx+0x9b0]    (the video)
        0x8B, 0x50, 0x60,                          // mov  edx, [rax+0x60]     (its state)
        0x83, 0xEA, 0x03,                          // sub  edx, 3
        0x83, 0xFA, 0x01,                          // cmp  edx, 1
        0x76, 0x19,                                // jbe  false               (state 3 or 4)
        0x48, 0x8B, 0x01,                          // mov  rax, [rcx]
        0xBA, 0x01, 0x00, 0x00, 0x00,              // mov  edx, 1
        0xFF, 0x90, 0xA8, 0x00, 0x00, 0x00,        // call [rax+0xa8] (HasFlag(1))
        0x84, 0xC0, 0x75, 0x07,                    // test al, al; jne false
        0xB0, 0x01, 0x48, 0x83, 0xC4, 0x28, 0xC3,  // true
        0x32, 0xC0, 0x48, 0x83, 0xC4, 0x28, 0xC3,  // false
    };
    constexpr std::uintptr_t kRenderSystemPtr2021 = 0x11228E8;
    constexpr std::uintptr_t kRenderSystemObj2021 = 0x1BD01F0;
    constexpr std::uintptr_t kRenderSystemVtbl2021 = 0xC879C8;

    // The Game Pass build: Epic's code at other addresses; the accessors and IsVideoDone have
    // Epic's bytes.
    constexpr std::uint32_t kTimeDateStamp2021GP = 0x6075C22A;
    constexpr std::uintptr_t kLoopChecks2021GP = 0x176AB7;
    constexpr std::uint8_t kLoopChecks2021GPBytes[] = {
        0x40, 0x84, 0xFF,                          // test dil, dil
        0x0F, 0x84, 0xB0, 0x01, 0x00, 0x00,        // je   0x176c70 (leave)
        0x0F, 0xB6, 0x86, 0xC0, 0x3F, 0x00, 0x00,  // movzx eax, byte [rsi+0x3fc0] (abort)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x85, 0xA1, 0x01, 0x00, 0x00,        // jne  0x176c70
        0xE8, 0xAC, 0x34, 0x94, 0x00,              // call 0xab9f80 (quitting?)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x85, 0x94, 0x01, 0x00, 0x00,        // jne  0x176c70
        0x44, 0x39, 0x35, 0xD5, 0x47, 0xAA, 0x01,  // cmp  [skipLoadingVideoAutomatically], r14d  <- hook
        0x0F, 0x85, 0x87, 0x01, 0x00, 0x00,        // jne  0x176c70
        0x48, 0x8B, 0x0D, 0xF8, 0x9D, 0x04, 0x01,  // mov  rcx, [renderSystem]
        0xBA, 0x08, 0x00, 0x00, 0x00,              // mov  edx, 8
        0x48, 0x8B, 0x01,                          // mov  rax, [rcx]
        0xFF, 0x90, 0xA8, 0x00, 0x00, 0x00,        // call [rax+0xa8] (HasFlag)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x84, 0x28, 0x01, 0x00, 0x00,        // je   0x176c2e
    };
    constexpr std::uintptr_t kHookAt2021GP = 0x176ADC;
    static_assert(kHookAt2021GP == kLoopChecks2021GP + 0x25);
    constexpr std::uintptr_t kLoopTail2021GP = 0x176C4E;
    constexpr std::uint8_t kLoopTail2021GPBytes[] = {
        0xB9, 0x0A, 0x00, 0x00, 0x00,              // mov  ecx, 10
        0xE8, 0xF8, 0x72, 0x8E, 0x00,              // call Sys_Sleep
        0x48, 0x8B, 0x0D, 0x89, 0x9C, 0x04, 0x01,  // mov  rcx, [renderSystem]
        0x48, 0x8B, 0x01,                          // mov  rax, [rcx]
        0xFF, 0x90, 0xB0, 0x00, 0x00, 0x00,        // call [rax+0xb0] (IsVideoDone)
        0x84, 0xC0,                                // test al, al
        0x0F, 0x84, 0x47, 0xFE, 0xFF, 0xFF,        // je   0x176ab7 (loop)
    };
    constexpr std::uintptr_t kSetFlag2021GP = 0x464E90;
    constexpr std::uintptr_t kClearFlag2021GP = 0x464AE0;
    constexpr std::uintptr_t kHasFlag2021GP = 0x462E10;
    constexpr std::uintptr_t kIsVideoDone2021GP = 0x465F00;
    constexpr std::uintptr_t kRenderSystemPtr2021GP = 0x11C08E8;
    constexpr std::uintptr_t kRenderSystemObj2021GP = 0x1C73F40;
    constexpr std::uintptr_t kRenderSystemVtbl2021GP = 0xCFB018;

    // What Install checks before it hooks: code that must match byte for byte, and the render
    // system's addresses, whose pointer and vtable slots are static data.
    struct Gate
    {
        std::uintptr_t rva;
        std::span<const std::uint8_t> bytes;
        const char* what;
    };
    struct Build
    {
        std::uint32_t stamp;
        std::span<const Gate> gates;
        std::uintptr_t hookAt;
        std::uintptr_t renderSystemPtr, renderSystemObj, renderSystemVtbl;
        std::uintptr_t setFlag, clearFlag, hasFlag, isVideoDone;
    };
    constexpr Gate kGates[] = {
        {kLoopChecks, kLoopChecksBytes, "the wait loop"},
        {kLoopTail, kLoopTailBytes, "the wait loop's tail"},
        {kFlagFns, kFlagFnsBytes, "the flag accessors"},
        {kIsVideoDone, kIsVideoDoneBytes, "IsVideoDone"},
    };
    constexpr Gate kGates2021[] = {
        {kLoopChecks2021, kLoopChecks2021Bytes, "the wait loop"},
        {kLoopTail2021, kLoopTail2021Bytes, "the wait loop's tail"},
        {kSetFlag2021, kSetFlag2021Bytes, "SetFlag"},
        {kClearFlag2021, kClearFlag2021Bytes, "ClearFlag"},
        {kHasFlag2021, kHasFlag2021Bytes, "HasFlag"},
        {kIsVideoDone2021, kIsVideoDone2021Bytes, "IsVideoDone"},
    };
    constexpr Gate kGates2021GP[] = {
        {kLoopChecks2021GP, kLoopChecks2021GPBytes, "the wait loop"},
        {kLoopTail2021GP, kLoopTail2021GPBytes, "the wait loop's tail"},
        {kSetFlag2021GP, kSetFlag2021Bytes, "SetFlag"},
        {kClearFlag2021GP, kClearFlag2021Bytes, "ClearFlag"},
        {kHasFlag2021GP, kHasFlag2021Bytes, "HasFlag"},
        {kIsVideoDone2021GP, kIsVideoDone2021Bytes, "IsVideoDone"},
    };
    constexpr Build kBuilds[] = {
        {kTimeDateStamp, kGates, kHookAt, kRenderSystemPtr, kRenderSystemObj, kRenderSystemVtbl, kFlagFns,
         kFlagFns + 0x10, kFlagFns + 0x20, kIsVideoDone},
        {kTimeDateStamp2021, kGates2021, kHookAt2021, kRenderSystemPtr2021, kRenderSystemObj2021, kRenderSystemVtbl2021,
         kSetFlag2021, kClearFlag2021, kHasFlag2021, kIsVideoDone2021},
        {kTimeDateStamp2021GP, kGates2021GP, kHookAt2021GP, kRenderSystemPtr2021GP, kRenderSystemObj2021GP,
         kRenderSystemVtbl2021GP, kSetFlag2021GP, kClearFlag2021GP, kHasFlag2021GP, kIsVideoDone2021GP},
    };

    // The load screen's flags (the render system's flag word at +0xc), as this plugin reads them.
    constexpr std::uint32_t kUp = 0x1;          // load screen up (IsVideoDone waits for it to clear)
    constexpr std::uint32_t kEarlyPress = 0x2;  // a continue key pressed while loading
    constexpr std::uint32_t kAccepting = 0x4;   // a press may skip the loading video
    constexpr std::uint32_t kInput = 0x8;       // the wait loop reads input
    constexpr std::uint32_t kLoadDone = 0x10;   // set just before the wait loop
    constexpr std::uint32_t kVideo = 0x20;      // a Bink loading video plays
    constexpr std::uint32_t kPress = kUp | kEarlyPress | kAccepting;  // what an accepted press clears

    const char* ToString(Result r)
    {
        switch (r) {
        case Result::Installed: return "hooked";
        case Result::UnknownBuild: return "unknown build, nothing changed";
        default: return "failed, nothing changed";
        }
    }

    const char* ToString(Decision d)
    {
        switch (d) {
        case Decision::Continue: return "loading done, no loading video: continuing";
        case Decision::Loading: return "still loading: waiting";
        case Decision::NotTakingInput: return "the game takes no input here: waiting";
        case Decision::VideoPlaying: return "a loading video plays: left to the player";
        default: return "already leaving";
        }
    }

    Decision Decide(std::uint32_t flags, int videoState)
    {
        // A video first: while a loading cutscene plays, the game keeps flag 1 clear (flags 0x38,
        // video state 3), and the log should say what is going on. None of the
        // decisions before Continue acts, so their order only changes the log line.
        if ((flags & kVideo) || videoState == 3 || videoState == 4) return Decision::VideoPlaying;
        if (!(flags & kUp)) return Decision::AlreadyLeaving;  // pressed (or continued): the loop leaves by itself
        if (!(flags & kLoadDone)) return Decision::Loading;   // not expected in this loop; never act early
        if (!(flags & kInput)) return Decision::NotTakingInput;  // a press would not count either
        return Decision::Continue;
    }

    static std::uint8_t* gBase = nullptr;
    static std::uintptr_t gRenderSystemPtr = 0, gRenderSystemVtbl = 0;  // the build's; set in Install
    static ULONGLONG gLastCall = 0;
    static Decision gLastDecision = Decision::AlreadyLeaving;
    static unsigned gContinued = 0;
    static bool gWarnedObject = false;

    // Runs on the game's main thread at the top of every iteration of the wait loop (~every 10 ms,
    // only while a load screen waits). Logs once per wait and when the decision changes.
    static void OnLoopCheck(SafetyHookContext&)
    {
        auto* rs = *reinterpret_cast<std::uint8_t**>(gBase + gRenderSystemPtr);
        if (!rs || *reinterpret_cast<std::uint8_t**>(rs) != gBase + gRenderSystemVtbl) {
            if (!gWarnedObject) {
                LOG_WARN("Load screen: the render system at {} is not the expected object: doing nothing.",
                         static_cast<void*>(rs));
                gWarnedObject = true;
            }
            return;
        }
        const std::uint32_t flags = *reinterpret_cast<volatile std::uint32_t*>(rs + kFlagsOffset);
        const auto* video = *reinterpret_cast<std::uint8_t* const*>(rs + kVideoOffset);
        const int state = video ? *reinterpret_cast<const volatile int*>(video + kVideoStateOffset) : 0;
        const Decision d = Decide(flags, state);

        const ULONGLONG now = GetTickCount64();
        const bool newWait = now - gLastCall > 500;  // iterations are ~10 ms apart
        gLastCall = now;
        if (newWait) {
            const ReadWaitFix::WaitStats reads = ReadWaitFix::TakeStats();  // all zero without SmartWait
            if (reads.waits)
                LOG_INFO("FastReads since the last load screen: {} reads waited for ({} seen finishing), {} of them "
                         "past the yielding time (then slept), longest {:.1f} ms",
                         reads.waits, reads.seenDone, reads.slept, reads.longestMs);
        }
        if (newWait || d != gLastDecision) {
            LOG_INFO("Load screen: {} (flags 0x{:x}, video state {})", ToString(d), flags, state);
            gLastDecision = d;
        }
        if (d != Decision::Continue) return;

        // Exactly what an accepted press does (0x146267 / 0x8b0e6b): renderSystem->ClearFlag(7),
        // through the game's own function. The loop then leaves through IsVideoDone().
        auto** vtbl = *reinterpret_cast<void***>(rs);
        reinterpret_cast<void (*)(void*, std::uint32_t)>(vtbl[kSlotClearFlag])(rs, kPress);
        ++gContinued;
        LOG_INFO("Load screen: continued without the prompt ({} this session)", gContinued);
    }

    static std::string Hex(const std::uint8_t* p, std::size_t n)
    {
        std::string s;
        for (std::size_t i = 0; i < n; ++i) s += std::format("{:02x}", p[i]);
        return s;
    }

    static bool Matches(const std::uint8_t* base, const Gate& g)
    {
        if (std::memcmp(base + g.rva, g.bytes.data(), g.bytes.size()) == 0) return true;
        LOG_WARN("Auto-continue: {} at 0x{:x} differs from the analysed build ({}): no hook.", g.what, g.rva,
                 Hex(base + g.rva, g.bytes.size()));
        return false;
    }

    Result Install(HMODULE exe)
    {
        auto* base = reinterpret_cast<std::uint8_t*>(exe);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        const Build* b = nullptr;
        for (const Build& candidate : kBuilds)
            if (candidate.stamp == nt->FileHeader.TimeDateStamp) b = &candidate;
        if (!b) {
            LOG_WARN("Auto-continue: not the analysed build (PE timestamp 0x{:08x}): no hook.", nt->FileHeader.TimeDateStamp);
            return Result::UnknownBuild;
        }
        if (base[b->hookAt] == 0xE9) {
            LOG_WARN("Auto-continue: 0x{:x} already jumps elsewhere (a second copy of the plugin, or another mod): no hook.",
                     b->hookAt);
            return Result::UnknownBuild;
        }
        for (const Gate& g : b->gates)
            if (!Matches(base, g)) return Result::UnknownBuild;

        // Static data, already relocated: the renderSystem pointer and four vtable slots.
        auto* rs = *reinterpret_cast<std::uint8_t**>(base + b->renderSystemPtr);
        auto** vtbl = reinterpret_cast<std::uint8_t**>(base + b->renderSystemVtbl);
        if (rs != base + b->renderSystemObj || vtbl[kSlotSetFlag] != base + b->setFlag ||
            vtbl[kSlotClearFlag] != base + b->clearFlag || vtbl[kSlotHasFlag] != base + b->hasFlag ||
            vtbl[kSlotIsVideoDone] != base + b->isVideoDone) {
            LOG_WARN("Auto-continue: the render system pointer or vtable differs from the analysed build: no hook.");
            return Result::UnknownBuild;
        }

        gBase = base;
        gRenderSystemPtr = b->renderSystemPtr;
        gRenderSystemVtbl = b->renderSystemVtbl;
        auto hook = safetyhook::MidHook::create(base + b->hookAt, OnLoopCheck);
        if (!hook) {
            gBase = nullptr;
            LOG_ERROR("Auto-continue: hook at 0x{:x} failed (safetyhook error {}): nothing changed.", b->hookAt,
                      static_cast<int>(hook.error().type));
            return Result::Failed;
        }
        // Never unhooked: it lives as long as the game. Leaked on purpose, so nothing of this
        // plugin runs or rewrites game code during the game's shutdown.
        new SafetyHookMid(std::move(*hook));
        LOG_INFO("Hooked 0x{:x}: after a load, the game continues without \"press X\" unless a loading video plays.",
                 b->hookAt);
        return Result::Installed;
    }
}
