#include "stdafx.h"
#include "intro_skip.h"

// ---------------------------------------------------------------------------
// At start-up the resource manager's Init2 (RVA 0x16ccf0) plays bink/loadvideo_pc_xboxone.bik
// (936 frames at 30 fps: 31.2 s) while the "common" block loads, then stays in the load-screen
// loop (0x166c50) until the video has ended; it can't be skipped. With FastReads, "common" loads in
// a few seconds, so the video is what holds the game.
//
// The game's own switch is the cvar com_skipIntroVideo ("skips the intro video", default "0").
// idCommonLocal::Init reads it once (0x1337ad), after the command line has been applied; if it is
// set, Init2 gets skip = true: no video during "common", no wait. Launching with
// +com_skipIntroVideo 1 does the same; this makes it the default, so no launch option is needed.
//
// The cvar is a static idCVar at 0x161e220 (.bss), built by this static initialiser:
//
//   0xb75b00  sub   rsp, 0x38
//   0xb75b04  lea   rax, [rip+...]          ; "skips the intro video"
//   0xb75b0b  lea   r8,  [rip+0x2e97ee]     ; "0" at 0xe5f300: the default
//   0xb75b12  lea   rdx, [rip+...]          ; "com_skipIntroVideo"
//   0xb75b19  lea   rcx, [rip+...]          ; the cvar, 0x161e220
//   0xb75b20  mov   r9d, 1                  ; flags
//   0xb75b26  mov   qword ptr [rsp+0x28], 0
//   0xb75b2f  mov   qword ptr [rsp+0x20], rax
//   0xb75b34  call  idCVar::idCVar          ; 0x9deb30: value string = the default
//   0xb75b39  lea   rcx, [rip+...]          ; its destructor
//   0xb75b40  add   rsp, 0x38
//   0xb75b44  jmp   atexit
//
// SkipIntroVideo points the default at the "1" the exe stores right after that "0" (0xe5f304), so
// the cvar is created as 1: four bytes, the lea's displacement. Nothing else changes: a
// "+com_skipIntroVideo 0" on the command line still turns it off, and the cvar isn't archived, so
// nothing is written to wolfConfig.cfg.
//
// This only works before the C runtime runs the exe's static initialisers. Ultimate ASI Loader
// loads the plugin before that, while the cvar's memory is still all zero, which is checked. If the
// cvar already exists, the plugin changes nothing and says so.
//
// The 2021 build (Epic, see read_wait_fix.cpp): the initialiser is at 0x21c90, its instructions in
// a different order; the default's lea is at 0x21caf. Its linker merges identical strings, so the
// "0" (0xbfc968) and the "1" (0xbfd0a0) are shared by many cvars and not next to the name. The
// Game Pass build has the same initialiser at 0x21d50.
// ---------------------------------------------------------------------------

#include <array>
#include <span>

namespace IntroSkip
{
    constexpr std::uint32_t kTimeDateStamp = 0x53A185CF;  // same build as read_wait_fix.cpp

    constexpr std::uintptr_t kInit = 0xB75B00;  // the static initialiser above
    constexpr std::uint8_t kInitBytes[] = {
        0x48, 0x83, 0xEC, 0x38,                                // sub  rsp, 0x38
        0x48, 0x8D, 0x05, 0xDD, 0x97, 0x2E, 0x00,              // lea  rax, [rip+0x2e97dd]
        0x4C, 0x8D, 0x05, 0xEE, 0x97, 0x2E, 0x00,              // lea  r8,  [rip+0x2e97ee]
        0x48, 0x8D, 0x15, 0xEF, 0x97, 0x2E, 0x00,              // lea  rdx, [rip+0x2e97ef]
        0x48, 0x8D, 0x0D, 0x00, 0x87, 0xAA, 0x00,              // lea  rcx, [rip+0xaa8700]
        0x41, 0xB9, 0x01, 0x00, 0x00, 0x00,                    // mov  r9d, 1
        0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00,  // mov  qword ptr [rsp+0x28], 0
        0x48, 0x89, 0x44, 0x24, 0x20,                          // mov  qword ptr [rsp+0x20], rax
        0xE8, 0xF7, 0x8F, 0xE6, 0xFF,                          // call 0x9deb30
        0x48, 0x8D, 0x0D, 0xD0, 0x73, 0x05, 0x00,              // lea  rcx, [rip+0x573d0]
        0x48, 0x83, 0xC4, 0x38,                                // add  rsp, 0x38
        0xE9, 0xD7, 0xC6, 0xF4, 0xFF,                          // jmp  0xac2220
    };
    constexpr std::size_t kDispOffset = 0x0E;      // disp32 of `lea r8, [rip+x]`, at 0xb75b0e
    constexpr std::uintptr_t kDispEnd = 0xB75B12;  // rip-relative to the next instruction
    constexpr std::uintptr_t kZero = 0xE5F300;     // "0"
    constexpr std::uintptr_t kOne = 0xE5F304;      // "1"
    constexpr std::uintptr_t kName = 0xE5F308;     // "com_skipIntroVideo"
    constexpr std::int32_t kOriginalDisp = static_cast<std::int32_t>(kZero - kDispEnd);
    constexpr std::int32_t kPatchedDisp = static_cast<std::int32_t>(kOne - kDispEnd);
    constexpr std::uintptr_t kCVar = 0x161E220;    // the idCVar object
    constexpr std::size_t kCVarSize = 0x78;        // idStr value .. next-in-list at +0x70
    static_assert(kInit + kDispOffset + 4 == kDispEnd);
    static_assert(kOriginalDisp == 0x2E97EE);

    // The 2021 build (Epic).
    constexpr std::uint32_t kTimeDateStamp2021 = 0x611A423D;
    constexpr std::uintptr_t kInit2021 = 0x21C90;
    constexpr std::uint8_t kInit2021Bytes[] = {
        0x48, 0x83, 0xEC, 0x38,                                // sub  rsp, 0x38
        0x48, 0x8D, 0x05, 0x0D, 0xDA, 0xBE, 0x00,              // lea  rax, [rip+0xbeda0d]  ("skips the intro video")
        0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00,  // mov  qword ptr [rsp+0x28], 0
        0x41, 0xB9, 0x01, 0x00, 0x00, 0x00,                    // mov  r9d, 1
        0x48, 0x89, 0x44, 0x24, 0x20,                          // mov  qword ptr [rsp+0x20], rax
        0x4C, 0x8D, 0x05, 0xB2, 0xAC, 0xBD, 0x00,              // lea  r8,  [rip+0xbdacb2]  ("0")
        0x48, 0x8D, 0x15, 0x03, 0xDA, 0xBE, 0x00,              // lea  rdx, [rip+0xbeda03]  ("com_skipIntroVideo")
        0x48, 0x8D, 0x0D, 0xAC, 0x03, 0x4A, 0x01,              // lea  rcx, [rip+0x14a03ac] (the cvar)
        0xE8, 0x17, 0x85, 0xA2, 0x00,                          // call 0xa4a1e0
        0x48, 0x8D, 0x0D, 0x30, 0x45, 0xBB, 0x00,              // lea  rcx, [rip+0xbb4530]
        0x48, 0x83, 0xC4, 0x38,                                // add  rsp, 0x38
        0xE9, 0xB3, 0xF7, 0xB6, 0x00,                          // jmp  0xb9148c
    };
    constexpr std::size_t kDispOffset2021 = 0x22;   // at 0x21cb2
    constexpr std::uintptr_t kZero2021 = 0xBFC968;  // "0"
    constexpr std::uintptr_t kOne2021 = 0xBFD0A0;   // "1"
    constexpr std::uintptr_t kName2021 = 0xC0F6C0;  // "com_skipIntroVideo"
    constexpr std::uintptr_t kCVar2021 = 0x14C2070;
    static_assert(kZero2021 - (kInit2021 + kDispOffset2021 + 4) == 0xBDACB2);  // the lea's disp above

    // The Game Pass build: Epic's code at other addresses.
    constexpr std::uint32_t kTimeDateStamp2021GP = 0x6075C22A;
    constexpr std::uintptr_t kInit2021GP = 0x21D50;
    constexpr std::uint8_t kInit2021GPBytes[] = {
        0x48, 0x83, 0xEC, 0x38,                                // sub  rsp, 0x38
        0x48, 0x8D, 0x05, 0x2D, 0xC1, 0xC5, 0x00,              // lea  rax, [rip+0xc5c12d]  ("skips the intro video")
        0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00,  // mov  qword ptr [rsp+0x28], 0
        0x41, 0xB9, 0x01, 0x00, 0x00, 0x00,                    // mov  r9d, 1
        0x48, 0x89, 0x44, 0x24, 0x20,                          // mov  qword ptr [rsp+0x20], rax
        0x4C, 0x8D, 0x05, 0x72, 0x93, 0xC4, 0x00,              // lea  r8,  [rip+0xc49372]  ("0")
        0x48, 0x8D, 0x15, 0x23, 0xC1, 0xC5, 0x00,              // lea  rdx, [rip+0xc5c123]  ("com_skipIntroVideo")
        0x48, 0x8D, 0x0D, 0xEC, 0x3E, 0x54, 0x01,              // lea  rcx, [rip+0x1543eec] (the cvar)
        0xE8, 0xA7, 0xF1, 0xA3, 0x00,                          // call 0xa60f30
        0x48, 0x8D, 0x0D, 0x20, 0x24, 0xC2, 0x00,              // lea  rcx, [rip+0xc22420]
        0x48, 0x83, 0xC4, 0x38,                                // add  rsp, 0x38
        0xE9, 0xB7, 0x46, 0xBD, 0x00,                          // jmp  0xbf6450
    };
    constexpr std::uintptr_t kZero2021GP = 0xC6B0E8;  // "0"
    constexpr std::uintptr_t kOne2021GP = 0xC6B820;   // "1"
    constexpr std::uintptr_t kName2021GP = 0xC7DEA0;  // "com_skipIntroVideo"
    constexpr std::uintptr_t kCVar2021GP = 0x1565C70;
    static_assert(kZero2021GP - (kInit2021GP + kDispOffset2021 + 4) == 0xC49372);

    struct Build
    {
        std::uint32_t stamp;
        std::uintptr_t init;
        std::span<const std::uint8_t> initBytes;
        std::size_t dispOffset;
        std::uintptr_t zero, one, name;  // the strings "0", "1", "com_skipIntroVideo"
        std::uintptr_t cvar;
    };
    constexpr Build kBuilds[] = {
        {kTimeDateStamp, kInit, kInitBytes, kDispOffset, kZero, kOne, kName, kCVar},
        {kTimeDateStamp2021, kInit2021, kInit2021Bytes, kDispOffset2021, kZero2021, kOne2021, kName2021, kCVar2021},
        {kTimeDateStamp2021GP, kInit2021GP, kInit2021GPBytes, kDispOffset2021, kZero2021GP, kOne2021GP, kName2021GP,
         kCVar2021GP},
    };
    constexpr std::size_t kMaxInit = 96;
    static_assert(sizeof(kInitBytes) <= kMaxInit && sizeof(kInit2021Bytes) <= kMaxInit &&
                  sizeof(kInit2021GPBytes) <= kMaxInit);

    static std::string Hex(const std::uint8_t* p, std::size_t n)
    {
        std::string s;
        for (std::size_t i = 0; i < n; ++i) s += std::format("{:02x}", p[i]);
        return s;
    }

    Result Apply(HMODULE exe)
    {
        auto* base = reinterpret_cast<std::uint8_t*>(exe);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        const Build* b = nullptr;
        for (const Build& candidate : kBuilds)
            if (candidate.stamp == nt->FileHeader.TimeDateStamp) b = &candidate;
        if (!b) {
            LOG_WARN("Intro video: not the analysed build (PE timestamp 0x{:08x}): changing nothing.",
                     nt->FileHeader.TimeDateStamp);
            return Result::UnknownBuild;
        }
        const std::uintptr_t dispEnd = b->init + b->dispOffset + 4;  // rip-relative to the next instruction
        const auto originalDisp = static_cast<std::int32_t>(static_cast<std::intptr_t>(b->zero - dispEnd));
        const auto patchedDisp = static_cast<std::int32_t>(static_cast<std::intptr_t>(b->one - dispEnd));

        // The initialiser must match byte for byte, except the default's displacement, which may
        // already be ours; and the strings it points at must be what they were.
        const std::size_t n = b->initBytes.size();
        std::array<std::uint8_t, kMaxInit> init{};
        std::memcpy(init.data(), base + b->init, n);
        std::int32_t disp = 0;
        std::memcpy(&disp, init.data() + b->dispOffset, sizeof(disp));
        std::memcpy(init.data() + b->dispOffset, b->initBytes.data() + b->dispOffset, sizeof(disp));
        if (std::memcmp(init.data(), b->initBytes.data(), n) != 0 || std::memcmp(base + b->zero, "0", 2) != 0 ||
            std::memcmp(base + b->one, "1", 2) != 0 || std::memcmp(base + b->name, "com_skipIntroVideo", 19) != 0) {
            LOG_WARN("Intro video: the initialiser of com_skipIntroVideo at 0x{:x} differs from the analysed build ({}): "
                     "changing nothing.", b->init, Hex(base + b->init, n));
            return Result::UnknownBuild;
        }
        if (disp == patchedDisp) {
            LOG_INFO("Intro video: com_skipIntroVideo already defaults to 1: nothing to do.");
            return Result::AlreadyPatched;
        }
        if (disp != originalDisp) {
            LOG_WARN("Intro video: the default of com_skipIntroVideo points at 0x{:x}, not 0x{:x}: changing nothing.",
                     dispEnd + disp, b->zero);
            return Result::UnknownBuild;
        }

        // Not built yet? Then its memory (.bss) is still all zero.
        for (std::size_t i = 0; i < kCVarSize; ++i) {
            if (base[b->cvar + i] != 0) {
                LOG_WARN("Intro video: com_skipIntroVideo already exists (the plugin was loaded after the game's "
                         "start-up code), so changing its default would do nothing: changing nothing.");
                return Result::Failed;
            }
        }

        std::uint8_t* target = base + b->init + b->dispOffset;
        DWORD oldProtect = 0;
        if (!VirtualProtect(target, sizeof(disp), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            LOG_ERROR("Intro video: VirtualProtect failed (error {}): changing nothing.", GetLastError());
            return Result::Failed;
        }
        std::memcpy(target, &patchedDisp, sizeof(patchedDisp));
        DWORD unused = 0;
        if (!VirtualProtect(target, sizeof(disp), oldProtect, &unused))
            LOG_WARN("Intro video: could not restore the protection at 0x{:x} (error {}); the change itself is made.",
                     b->init + b->dispOffset, GetLastError());
        FlushInstructionCache(GetCurrentProcess(), target, sizeof(disp));

        if (std::memcmp(target, &patchedDisp, sizeof(patchedDisp)) != 0) {
            LOG_ERROR("Intro video: write did not stick at 0x{:x}.", b->init + b->dispOffset);
            return Result::Failed;
        }
        LOG_INFO("Patched 0x{:x}: com_skipIntroVideo starts as 1 instead of 0, as with +com_skipIntroVideo 1 "
                 "(no logo video at start-up).", b->init + b->dispOffset);
        return Result::Patched;
    }
}
