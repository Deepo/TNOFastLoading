#include "stdafx.h"
#include "quit_fix.h"

// ---------------------------------------------------------------------------
// The Old Blood only. At exit (the menu's Quit to Desktop, or a fatal error) the game shuts down its
// virtual-texture system (0x4a98c0, on the object at 0x1f295d0), which first empties each of its
// four physical page pools with idPhysicalPages::EmptyCache (0x4a5240; also the console command
// vt_emptyCache, "empties and zeros the physical images"). EmptyCache builds one blank 128x128 page
// and uploads it into every page slot of the pool's two or three page images, one SubImageUpload
// (0x4045e0) per page. The images are deleted right afterwards:
//
//   0x4a9a43  lea   rdi, [r13+0xb820]           ; the first pool
//   0x4a9a4a  mov   ebp, 4
//   0x4a9a4f  mov   rbx, rdi
//   0x4a9a52  mov   esi, ebp
//   0x4a9a54  mov   rcx, rbx
//   0x4a9a57  call  0x4a5240                    ; EmptyCache(pool)
//   0x4a9a5c  add   rbx, 0x100                  ; the next pool
//   0x4a9a63  dec   rsi
//   0x4a9a66  jne   0x4a9a54
//
// The shutdown runs twice: from idCommonLocal's shutdown (0x12fb70, common vtable slot 3, which
// runs once) and again from idRenderSystem::Shutdown (0x48a1a0, render vtable slot 3), whose only
// callers are that shutdown and the fatal-error path (0x508200). So this is exit-only code.
//
// The console times each pass above 100 ms ("timer: idPhysicalPages::EmptyCache SubImageUpload: N
// pages: M ms"). With the default page images (4096x4096 and 8192x8192: 1,024 and 4,096 pages) the
// passes took about 1 s at Quit to Desktop (Steam, 2026-10-08). With the image sizes raised to 16384
// in base\graphicsprofiles.json (a tweak for sharper textures), five passes of 16,384 pages took
// about 10 s (GOG). Closing the window instead (WM_CLOSE: the quit command) took 0.05-0.09 s on
// Steam with the default sizes; why that route is faster wasn't examined.
//
// FastQuit replaces the call with a 5-byte nop: the loop still runs, nothing is blanked. The call
// only clobbered volatile registers, and nothing after the loop uses what it left in them.
//
// The Game Pass build (2021): the same loop at 0x4c9fc5 (`sub rsi, 1` instead of `dec rsi`), the
// call at 0x4c9fd9 to EmptyCache at 0x4c5900, in the shutdown at 0x4c9e40, which is called from
// idCommonLocal's shutdown (0x13cc60) and idRenderSystem::Shutdown (0x4aa450, whose only other
// caller is the fatal-error path, 0x33dd90).
//
// The New Order's recorded quits print no such timer, so there is no row for it.
// ---------------------------------------------------------------------------

#include <array>
#include <span>

namespace QuitFix
{
    // The Old Blood (GOG, Steam).
    constexpr std::uint32_t kTimeDateStampTOB = 0x554C7C23;
    constexpr std::uintptr_t kLoopTOB = 0x4A9A43;  // the loop above
    constexpr std::uint8_t kLoopTOBBytes[] = {
        0x49, 0x8D, 0xBD, 0x20, 0xB8, 0x00, 0x00,  // lea  rdi, [r13+0xb820]
        0xBD, 0x04, 0x00, 0x00, 0x00,              // mov  ebp, 4
        0x48, 0x8B, 0xDF,                          // mov  rbx, rdi
        0x8B, 0xF5,                                // mov  esi, ebp
        0x48, 0x8B, 0xCB,                          // mov  rcx, rbx
        0xE8, 0xE4, 0xB7, 0xFF, 0xFF,              // call 0x4a5240 (EmptyCache)
        0x48, 0x81, 0xC3, 0x00, 0x01, 0x00, 0x00,  // add  rbx, 0x100
        0x48, 0xFF, 0xCE,                          // dec  rsi
        0x75, 0xEC,                                // jne  0x4a9a54
    };
    constexpr std::size_t kCallOffset = 0x14;  // the call, at 0x4a9a57
    static_assert(kLoopTOB + kCallOffset == 0x4A9A57);
    // EmptyCache's first instructions, so the call removed is that one.
    constexpr std::uintptr_t kEmptyCacheTOB = 0x4A5240;
    constexpr std::uint8_t kEmptyCacheTOBBytes[] = {
        0x40, 0x57,                    // push rdi
        0x41, 0x56,                    // push r14
        0x41, 0x57,                    // push r15
        0xB8, 0xA0, 0x40, 0x00, 0x00,  // mov  eax, 0x40a0 (its stack frame)
    };

    // The Old Blood, Game Pass.
    constexpr std::uint32_t kTimeDateStampTOBGP = 0x60770B00;
    constexpr std::uintptr_t kLoopTOBGP = 0x4C9FC5;
    constexpr std::uint8_t kLoopTOBGPBytes[] = {
        0x49, 0x8D, 0xBD, 0x20, 0xB8, 0x00, 0x00,  // lea  rdi, [r13+0xb820]
        0xBD, 0x04, 0x00, 0x00, 0x00,              // mov  ebp, 4
        0x48, 0x8B, 0xDF,                          // mov  rbx, rdi
        0x8B, 0xF5,                                // mov  esi, ebp
        0x48, 0x8B, 0xCB,                          // mov  rcx, rbx
        0xE8, 0x22, 0xB9, 0xFF, 0xFF,              // call 0x4c5900 (EmptyCache)
        0x48, 0x81, 0xC3, 0x00, 0x01, 0x00, 0x00,  // add  rbx, 0x100
        0x48, 0x83, 0xEE, 0x01,                    // sub  rsi, 1
        0x75, 0xEB,                                // jne  0x4c9fd6
    };
    static_assert(kLoopTOBGP + kCallOffset == 0x4C9FD9);
    constexpr std::uintptr_t kEmptyCacheTOBGP = 0x4C5900;
    constexpr std::uint8_t kEmptyCacheTOBGPBytes[] = {
        0x40, 0x57,                    // push rdi
        0x41, 0x56,                    // push r14
        0x41, 0x57,                    // push r15
        0xB8, 0x90, 0x40, 0x00, 0x00,  // mov  eax, 0x4090
    };

    constexpr std::uint8_t kNop5[] = {0x0F, 0x1F, 0x44, 0x00, 0x00};  // nop dword ptr [rax+rax*1+0]

    struct Build
    {
        std::uint32_t stamp;
        std::uintptr_t loop;
        std::span<const std::uint8_t> loopBytes;
        std::uintptr_t emptyCache;
        std::span<const std::uint8_t> emptyCacheBytes;
    };
    constexpr Build kBuilds[] = {
        {kTimeDateStampTOB, kLoopTOB, kLoopTOBBytes, kEmptyCacheTOB, kEmptyCacheTOBBytes},
        {kTimeDateStampTOBGP, kLoopTOBGP, kLoopTOBGPBytes, kEmptyCacheTOBGP, kEmptyCacheTOBGPBytes},
    };
    constexpr std::size_t kMaxLoop = 48;
    static_assert(sizeof(kLoopTOBBytes) <= kMaxLoop && sizeof(kLoopTOBGPBytes) <= kMaxLoop);

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
            LOG_WARN("Quit: not the analysed build (PE timestamp 0x{:08x}): changing nothing.", nt->FileHeader.TimeDateStamp);
            return Result::UnknownBuild;
        }

        // The loop must match byte for byte, except the call, which may already be our nop; and the
        // call must go to EmptyCache.
        const std::size_t n = b->loopBytes.size();
        std::array<std::uint8_t, kMaxLoop> loop{};
        std::memcpy(loop.data(), base + b->loop, n);
        const bool nopped = std::memcmp(loop.data() + kCallOffset, kNop5, sizeof(kNop5)) == 0;
        std::memcpy(loop.data() + kCallOffset, b->loopBytes.data() + kCallOffset, sizeof(kNop5));
        if (std::memcmp(loop.data(), b->loopBytes.data(), n) != 0) {
            LOG_WARN("Quit: the texture-cache loop at 0x{:x} differs from the analysed build ({}): changing nothing.",
                     b->loop, Hex(base + b->loop, n));
            return Result::UnknownBuild;
        }
        if (nopped) {
            LOG_INFO("Quit: the texture caches are already not blanked at exit: nothing to do.");
            return Result::AlreadyPatched;
        }
        if (std::memcmp(base + b->emptyCache, b->emptyCacheBytes.data(), b->emptyCacheBytes.size()) != 0) {
            LOG_WARN("Quit: the function the loop calls at 0x{:x} differs from the analysed build ({}): changing nothing.",
                     b->emptyCache, Hex(base + b->emptyCache, b->emptyCacheBytes.size()));
            return Result::UnknownBuild;
        }

        std::uint8_t* target = base + b->loop + kCallOffset;
        DWORD oldProtect = 0;
        if (!VirtualProtect(target, sizeof(kNop5), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            LOG_ERROR("Quit: VirtualProtect failed (error {}): changing nothing.", GetLastError());
            return Result::Failed;
        }
        std::memcpy(target, kNop5, sizeof(kNop5));
        DWORD unused = 0;
        if (!VirtualProtect(target, sizeof(kNop5), oldProtect, &unused))
            LOG_WARN("Quit: could not restore the protection at 0x{:x} (error {}); the change itself is made.",
                     b->loop + kCallOffset, GetLastError());
        FlushInstructionCache(GetCurrentProcess(), target, sizeof(kNop5));

        if (std::memcmp(target, kNop5, sizeof(kNop5)) != 0) {
            LOG_ERROR("Quit: write did not stick at 0x{:x}.", b->loop + kCallOffset);
            return Result::Failed;
        }
        LOG_INFO("Patched 0x{:x}: at exit the game no longer blanks its texture caches page by page (EmptyCache at "
                 "0x{:x}) just before deleting them.", b->loop + kCallOffset, b->emptyCache);
        return Result::Patched;
    }
}
