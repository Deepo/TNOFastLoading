#include "stdafx.h"
#include "preload_wait_fix.h"

// ---------------------------------------------------------------------------
// After "N total seconds for map load", the map-load function (RVA 0x16c580) starts the
// virtual-texture preload of the map's material list and waits for it:
//
//   0x16c8ef  call  0x482e40                ; start PRELOAD_MATERIAL_LIST, returns its id
//   0x16c8f4  mov   ebx, eax
//   0x16c8f6  mov   edx, eax
//   0x16c8f8  lea   rcx, [rip+...]          ; the preloader (0x1d21ed8)
//   0x16c8ff  call  0x481f70                ; done?
//   0x16c904  test  al, al
//   0x16c906  jne   0x16c92c
//   0x16c908  nop
//   0x16c910  mov   ecx, 0x64
//   0x16c915  call  Sys_Sleep               ; Sleep(100)
//   0x16c91a  mov   edx, ebx
//   0x16c91c  lea   rcx, [rip+...]
//   0x16c923  call  0x481f70                ; done?
//   0x16c928  test  al, al
//   0x16c92a  je    0x16c910
//
// The first check comes right after the start, so it always fails, and every load then sleeps a
// full 100 ms, although the game logs the preload as "0.0 seconds for PRELOAD_MATERIAL_LIST".
//
// FastPreloadWait changes the sleep's argument from 100 to 1 (the immediate at 0x16c911), written
// once in DllMain. The loop and its check stay as they are; it polls every 1-2 ms instead of every
// 100 ms.
//
// The 2021 build (Epic, see read_wait_fix.cpp) has the same loop at 0x1712b7, with a 5-byte nop
// instead of an 8-byte one, so the immediate is at 0x1712d1. Game Pass has the loop at 0x171877.
// The Old Blood's map load (0x163590) has it at 0x1638f5, with a 7-byte nop: the immediate is at
// 0x163911. Its Game Pass build has it at 0x172f69 with a 3-byte nop (the immediate at 0x172f81).
// ---------------------------------------------------------------------------

#include <array>
#include <span>

namespace PreloadWaitFix
{
    constexpr std::uint32_t kTimeDateStamp = 0x53A185CF;  // same build as read_wait_fix.cpp

    constexpr std::uintptr_t kLoop = 0x16C8F4;  // the code above, from `mov ebx, eax`
    constexpr std::uint8_t kLoopBytes[] = {
        0x8B, 0xD8,                                      // mov  ebx, eax
        0x8B, 0xD0,                                      // mov  edx, eax
        0x48, 0x8D, 0x0D, 0xD9, 0x55, 0xBB, 0x01,        // lea  rcx, [rip+0x1bb55d9]
        0xE8, 0x6C, 0x56, 0x31, 0x00,                    // call 0x481f70
        0x84, 0xC0,                                      // test al, al
        0x75, 0x24,                                      // jne  0x16c92c
        0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,  // nop
        0xB9, 0x64, 0x00, 0x00, 0x00,                    // mov  ecx, 0x64
        0xE8, 0x96, 0xF6, 0x86, 0x00,                    // call Sys_Sleep (0x9dbfb0)
        0x8B, 0xD3,                                      // mov  edx, ebx
        0x48, 0x8D, 0x0D, 0xB5, 0x55, 0xBB, 0x01,        // lea  rcx, [rip+0x1bb55b5]
        0xE8, 0x48, 0x56, 0x31, 0x00,                    // call 0x481f70
        0x84, 0xC0,                                      // test al, al
        0x74, 0xE4,                                      // je   0x16c910
    };
    constexpr std::size_t kImmOffset = 0x1D;  // imm32 of `mov ecx, 0x64`, at 0x16c911
    constexpr std::uint32_t kOriginalMs = 100;
    constexpr std::uint32_t kPatchedMs = 1;
    static_assert(kLoop + kImmOffset == 0x16C911);

    // The 2021 build (Epic).
    constexpr std::uint32_t kTimeDateStamp2021 = 0x611A423D;
    constexpr std::uintptr_t kLoop2021 = 0x1712B7;
    constexpr std::uint8_t kLoop2021Bytes[] = {
        0x8B, 0xD8,                                // mov  ebx, eax
        0x8B, 0xD0,                                // mov  edx, eax
        0x48, 0x8D, 0x0D, 0xE6, 0x64, 0xA6, 0x01,  // lea  rcx, [rip+0x1a664e6]
        0xE8, 0x19, 0xA2, 0x2F, 0x00,              // call 0x46b4e0
        0x84, 0xC0,                                // test al, al
        0x75, 0x21,                                // jne  0x1712ec
        0x0F, 0x1F, 0x44, 0x00, 0x00,              // nop
        0xB9, 0x64, 0x00, 0x00, 0x00,              // mov  ecx, 0x64
        0xE8, 0x26, 0x5F, 0x8D, 0x00,              // call Sys_Sleep (0xa47200)
        0x8B, 0xD3,                                // mov  edx, ebx
        0x48, 0x8D, 0x0D, 0xC5, 0x64, 0xA6, 0x01,  // lea  rcx, [rip+0x1a664c5]
        0xE8, 0xF8, 0xA1, 0x2F, 0x00,              // call 0x46b4e0
        0x84, 0xC0,                                // test al, al
        0x74, 0xE4,                                // je   0x1712d0
    };
    constexpr std::size_t kImmOffset2021 = 0x1A;  // at 0x1712d1
    static_assert(kLoop2021 + kImmOffset2021 == 0x1712D1);

    // The Game Pass build: Epic's code at other addresses.
    constexpr std::uint32_t kTimeDateStamp2021GP = 0x6075C22A;
    constexpr std::uintptr_t kLoop2021GP = 0x171877;
    constexpr std::uint8_t kLoop2021GPBytes[] = {
        0x8B, 0xD8,                                // mov  ebx, eax
        0x8B, 0xD0,                                // mov  edx, eax
        0x48, 0x8D, 0x0D, 0x76, 0x9C, 0xB0, 0x01,  // lea  rcx, [rip+0x1b09c76]
        0xE8, 0x79, 0x19, 0x31, 0x00,              // call 0x483200
        0x84, 0xC0,                                // test al, al
        0x75, 0x21,                                // jne  0x1718ac
        0x0F, 0x1F, 0x44, 0x00, 0x00,              // nop
        0xB9, 0x64, 0x00, 0x00, 0x00,              // mov  ecx, 0x64
        0xE8, 0xB6, 0xC6, 0x8E, 0x00,              // call Sys_Sleep (0xa5df50)
        0x8B, 0xD3,                                // mov  edx, ebx
        0x48, 0x8D, 0x0D, 0x55, 0x9C, 0xB0, 0x01,  // lea  rcx, [rip+0x1b09c55]
        0xE8, 0x58, 0x19, 0x31, 0x00,              // call 0x483200
        0x84, 0xC0,                                // test al, al
        0x74, 0xE4,                                // je   0x171890
    };
    static_assert(kLoop2021GP + kImmOffset2021 == 0x171891);

    // The Old Blood (GOG).
    constexpr std::uint32_t kTimeDateStampTOB = 0x554C7C23;
    constexpr std::uintptr_t kLoopTOB = 0x1638F5;
    constexpr std::uint8_t kLoopTOBBytes[] = {
        0x8B, 0xD8,                                // mov  ebx, eax
        0x8B, 0xD0,                                // mov  edx, eax
        0x48, 0x8D, 0x0D, 0xA8, 0x54, 0xDC, 0x01,  // lea  rcx, [rip+0x1dc54a8]
        0xE8, 0xBB, 0xD8, 0x33, 0x00,              // call 0x4a11c0
        0x84, 0xC0,                                // test al, al
        0x75, 0x23,                                // jne  0x16392c
        0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00,  // nop
        0xB9, 0x64, 0x00, 0x00, 0x00,              // mov  ecx, 0x64
        0xE8, 0x86, 0x0E, 0x85, 0x00,              // call Sys_Sleep (0x9b47a0)
        0x8B, 0xD3,                                // mov  edx, ebx
        0x48, 0x8D, 0x0D, 0x85, 0x54, 0xDC, 0x01,  // lea  rcx, [rip+0x1dc5485]
        0xE8, 0x98, 0xD8, 0x33, 0x00,              // call 0x4a11c0
        0x84, 0xC0,                                // test al, al
        0x74, 0xE4,                                // je   0x163910
    };
    constexpr std::size_t kImmOffsetTOB = 0x1C;  // at 0x163911
    static_assert(kLoopTOB + kImmOffsetTOB == 0x163911);

    // The Old Blood, Game Pass.
    constexpr std::uint32_t kTimeDateStampTOBGP = 0x60770B00;
    constexpr std::uintptr_t kLoopTOBGP = 0x172F69;
    constexpr std::uint8_t kLoopTOBGPBytes[] = {
        0x8B, 0xD8,                                // mov  ebx, eax
        0x8B, 0xD0,                                // mov  edx, eax
        0x48, 0x8D, 0x0D, 0x54, 0xD1, 0xB2, 0x01,  // lea  rcx, [rip+0x1b2d154]
        0xE8, 0x57, 0xE7, 0x34, 0x00,              // call 0x4c16d0
        0x84, 0xC0,                                // test al, al
        0x75, 0x1F,                                // jne  0x172f9c
        0x0F, 0x1F, 0x00,                          // nop
        0xB9, 0x64, 0x00, 0x00, 0x00,              // mov  ecx, 0x64
        0xE8, 0x66, 0x2C, 0x8F, 0x00,              // call Sys_Sleep (0xa65bf0)
        0x8B, 0xD3,                                // mov  edx, ebx
        0x48, 0x8D, 0x0D, 0x35, 0xD1, 0xB2, 0x01,  // lea  rcx, [rip+0x1b2d135]
        0xE8, 0x38, 0xE7, 0x34, 0x00,              // call 0x4c16d0
        0x84, 0xC0,                                // test al, al
        0x74, 0xE4,                                // je   0x172f80
    };
    constexpr std::size_t kImmOffsetTOBGP = 0x18;  // at 0x172f81
    static_assert(kLoopTOBGP + kImmOffsetTOBGP == 0x172F81);

    struct Build
    {
        std::uint32_t stamp;
        std::uintptr_t loop;
        std::span<const std::uint8_t> loopBytes;
        std::size_t immOffset;
    };
    constexpr Build kBuilds[] = {
        {kTimeDateStamp, kLoop, kLoopBytes, kImmOffset},
        {kTimeDateStamp2021, kLoop2021, kLoop2021Bytes, kImmOffset2021},
        {kTimeDateStamp2021GP, kLoop2021GP, kLoop2021GPBytes, kImmOffset2021},
        {kTimeDateStampTOB, kLoopTOB, kLoopTOBBytes, kImmOffsetTOB},
        {kTimeDateStampTOBGP, kLoopTOBGP, kLoopTOBGPBytes, kImmOffsetTOBGP},
    };
    constexpr std::size_t kMaxLoop = 64;
    static_assert(sizeof(kLoopBytes) <= kMaxLoop && sizeof(kLoop2021Bytes) <= kMaxLoop &&
                  sizeof(kLoop2021GPBytes) <= kMaxLoop && sizeof(kLoopTOBBytes) <= kMaxLoop &&
                  sizeof(kLoopTOBGPBytes) <= kMaxLoop);

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
            LOG_WARN("Preload wait: not the analysed build (PE timestamp 0x{:08x}): changing nothing.",
                     nt->FileHeader.TimeDateStamp);
            return Result::UnknownBuild;
        }

        // The loop must match byte for byte, except the sleep's immediate, which may already be ours.
        const std::size_t n = b->loopBytes.size();
        std::array<std::uint8_t, kMaxLoop> loop{};
        std::memcpy(loop.data(), base + b->loop, n);
        std::uint32_t ms = 0;
        std::memcpy(&ms, loop.data() + b->immOffset, sizeof(ms));
        std::memcpy(loop.data() + b->immOffset, b->loopBytes.data() + b->immOffset, sizeof(ms));
        if (std::memcmp(loop.data(), b->loopBytes.data(), n) != 0) {
            LOG_WARN("Preload wait: the poll loop at 0x{:x} differs from the analysed build ({}): changing nothing.",
                     b->loop, Hex(base + b->loop, n));
            return Result::UnknownBuild;
        }
        if (ms == kPatchedMs) {
            LOG_INFO("Preload wait: the poll already sleeps {} ms: nothing to do.", kPatchedMs);
            return Result::AlreadyPatched;
        }
        if (ms != kOriginalMs) {
            LOG_WARN("Preload wait: the poll sleeps {} ms, not {}: changing nothing.", ms, kOriginalMs);
            return Result::UnknownBuild;
        }

        std::uint8_t* target = base + b->loop + b->immOffset;
        DWORD oldProtect = 0;
        if (!VirtualProtect(target, sizeof(ms), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            LOG_ERROR("Preload wait: VirtualProtect failed (error {}): changing nothing.", GetLastError());
            return Result::Failed;
        }
        std::memcpy(target, &kPatchedMs, sizeof(kPatchedMs));
        DWORD unused = 0;
        if (!VirtualProtect(target, sizeof(ms), oldProtect, &unused))
            LOG_WARN("Preload wait: could not restore the protection at 0x{:x} (error {}); the change itself is made.",
                     b->loop + b->immOffset, GetLastError());
        FlushInstructionCache(GetCurrentProcess(), target, sizeof(ms));

        if (std::memcmp(target, &kPatchedMs, sizeof(kPatchedMs)) != 0) {
            LOG_ERROR("Preload wait: write did not stick at 0x{:x}.", b->loop + b->immOffset);
            return Result::Failed;
        }
        LOG_INFO("Patched 0x{:x}: the material-preload poll after each map load sleeps {} ms per check instead of {} ms.",
                 b->loop + b->immOffset, kPatchedMs, kOriginalMs);
        return Result::Patched;
    }
}
