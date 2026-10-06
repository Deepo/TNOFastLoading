#include "stdafx.h"
#include "read_wait_fix.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <intrin.h>  // _AddressOfReturnAddress
#include <span>

#include <safetyhook.hpp>  // its allocator: memory within reach of the exe's rel32 call

// ---------------------------------------------------------------------------
// The game waits for each stream read in idStreamControlThread::UncachedBackgroundRead
// (RVA 0x173240): it queues the read for the StreamControl thread, wakes it, and polls a byte on
// its own stack until the read is done:
//
//   0x17336b  movzx r11d, byte [rsp+0x60]   ; read done?
//   0x173371  test  r11b, r11b
//   0x173374  jne   0x173393
//   0x173376  movzx eax, byte [rsi+0x10]    ; StreamControl shutting down?
//   0x17337a  test  al, al
//   0x17337c  jne   0x173393
//   0x17337e  mov   ecx, 1
//   0x173383  call  Sys_Sleep (0x9dbfb0)    ; Sleep(1)
//   0x173388  movzx r11d, byte [rsp+0x60]
//   0x17338e  test  r11b, r11b
//   0x173391  je    0x173376
//
// Each Sleep(1) lasts about 2 ms in this game, while the read itself takes microseconds on an
// SSD, and every caller of UncachedBackgroundRead waits this way: the sound loader (0x49e7e0,
// every Ogg sample, on the main thread), the background loader's block reader (0x16dd10, 64 KB
// blocks), the zlib resource read (0x168760) and the virtual-texture header read (0x480280).
//
// FastReads retargets that one call: four bytes, its rel32, written once in DllMain. The loop and
// both of its exit checks stay as they are; `mov ecx, 1` becomes a dead argument.
//  - Without SmartWait, the call goes to the exe's own SwitchToThread thunk (0x9dfbd0). The loop
//    yields between its checks instead of sleeping, and no code of this plugin runs afterwards.
//  - With SmartWait ([Loading] SmartWait, the default), the call goes to the plugin's own wait,
//    through a 17-byte jump in memory within reach of the exe (a rel32 call can't reach the .asi):
//    `mov rdx, rsi; jmp [rip+0]`, so the wait also gets the loop's rsi, the StreamControl thread
//    object. It waits for the whole read, checking what the loop checks between its sleeps: the
//    read's done flag, [rsp+0x60] of the loop's frame (0x68 above the wait's own return address),
//    and StreamControl's "shutting down" byte, [rsi+0x10]. It yields for the first 2 ms, then
//    sleeps 1 ms at a time like the original, so a slow hard drive's read doesn't keep a core
//    busy. When it returns, the loop sees the same flag and leaves: one call per read. It runs on
//    the game's reading threads, so it doesn't log or allocate; its counts go out through
//    TakeStats.
//
// Three builds of the exe are known, told apart by the PE timestamp:
//  - 2014, "Wolfenstein The New Order.1683.12274 win-x64 Retail Jun 18 2014": GOG's exe, and
//    Steam's, which is the same exe inside Steam's DRM wrapper (Ultimate ASI Loader starts the
//    plugin once the code is decrypted). The addresses above are this build's.
//  - 2021 (2021-08-16): Epic's exe, recompiled and laid out anew. The same function is at
//    0x17d2d0 with the same frame; its loop (0x17d3e5) reads the same done flag at [rsp+0x60],
//    only through eax instead of r11d and with a 2-byte nop.
//  - 2021GP (2021-04-13): the Game Pass exe, the same code as Epic's at other addresses (the loop
//    at 0x17d825).
// ---------------------------------------------------------------------------

namespace ReadWaitFix
{
    // WolfNewOrder_x64.exe, GOG, "Wolfenstein The New Order.1683.12274 win-x64 Retail Jun 18 2014".
    // Steam's exe is the same build.
    constexpr std::uint32_t kTimeDateStamp = 0x53A185CF;

    constexpr std::uintptr_t kLoop = 0x17336B;  // the poll loop above
    constexpr std::uint8_t kLoopBytes[] = {
        0x44, 0x0F, 0xB6, 0x5C, 0x24, 0x60,  // movzx r11d, byte [rsp+0x60]
        0x45, 0x84, 0xDB,                    // test  r11b, r11b
        0x75, 0x1D,                          // jne   0x173393
        0x0F, 0xB6, 0x46, 0x10,              // movzx eax, byte [rsi+0x10]
        0x84, 0xC0,                          // test  al, al
        0x75, 0x15,                          // jne   0x173393
        0xB9, 0x01, 0x00, 0x00, 0x00,        // mov   ecx, 1
        0xE8, 0x28, 0x8C, 0x86, 0x00,        // call  Sys_Sleep
        0x44, 0x0F, 0xB6, 0x5C, 0x24, 0x60,  // movzx r11d, byte [rsp+0x60]
        0x45, 0x84, 0xDB,                    // test  r11b, r11b
        0x74, 0xE3,                          // je    0x173376
    };
    constexpr std::size_t kRelOffset = 0x19;            // rel32 of the call at 0x173383
    constexpr std::uintptr_t kCallNext = kLoop + 0x1D;  // 0x173388, where rel32 counts from

    // Both thunks are `jmp qword ptr [rip+disp32]` through the import table.
    constexpr std::uintptr_t kSysSleep = 0x9DBFB0;
    constexpr std::uint8_t kSysSleepBytes[] = {0x48, 0xFF, 0x25, 0xA9, 0x23, 0x20, 0x00};  // -> IAT 0xbde360 Sleep
    constexpr std::uintptr_t kSysYield = 0x9DFBD0;
    constexpr std::uint8_t kSysYieldBytes[] = {0x48, 0xFF, 0x25, 0xB1, 0xE7, 0x1F, 0x00};  // -> IAT 0xbde388 SwitchToThread
    constexpr std::uintptr_t kIatSwitchToThread = 0xBDE388;

    constexpr std::int32_t kRelToSleep = static_cast<std::int32_t>(kSysSleep - kCallNext);
    constexpr std::int32_t kRelToYield = static_cast<std::int32_t>(kSysYield - kCallNext);
    static_assert(kRelToSleep == 0x00868C28 && kRelToYield == 0x0086C848);

    constexpr std::size_t kDoneFlag = 0x60;      // the loop's `movzx r11d, byte [rsp+0x60]`
    constexpr std::size_t kShutdownFlag = 0x10;  // the loop's `movzx eax, byte [rsi+0x10]`
    // SmartWait's jump: mov rdx, rsi (the StreamControl object, SmartWait's second argument; rdx
    // is free at a call); jmp qword [rip+0]; dq SmartWait.
    constexpr std::uint8_t kJumpHead[] = {0x48, 0x89, 0xF2, 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
    constexpr std::size_t kJumpSize = sizeof(kJumpHead) + 8;

    // The 2021 build (Epic).
    constexpr std::uint32_t kTimeDateStamp2021 = 0x611A423D;
    constexpr std::uintptr_t kLoop2021 = 0x17D3E5;
    constexpr std::uint8_t kLoop2021Bytes[] = {
        0x0F, 0xB6, 0x44, 0x24, 0x60,  // movzx eax, byte [rsp+0x60]
        0x84, 0xC0,                    // test  al, al
        0x75, 0x1D,                    // jne   0x17d40b
        0x66, 0x90,                    // nop
        0x0F, 0xB6, 0x46, 0x10,        // movzx eax, byte [rsi+0x10]
        0x84, 0xC0,                    // test  al, al
        0x75, 0x13,                    // jne   0x17d40b
        0xB9, 0x01, 0x00, 0x00, 0x00,  // mov   ecx, 1
        0xE8, 0xFE, 0x9D, 0x8C, 0x00,  // call  Sys_Sleep
        0x0F, 0xB6, 0x44, 0x24, 0x60,  // movzx eax, byte [rsp+0x60]
        0x84, 0xC0,                    // test  al, al
        0x74, 0xE5,                    // je    0x17d3f0
    };
    constexpr std::uintptr_t kSysSleep2021 = 0xA47200;
    constexpr std::uint8_t kSysSleep2021Bytes[] = {0x48, 0xFF, 0x25, 0x31, 0x41, 0x1A, 0x00};  // -> IAT 0xbeb338 Sleep
    constexpr std::uintptr_t kSysYield2021 = 0xA51730;
    constexpr std::uint8_t kSysYield2021Bytes[] = {0x48, 0xFF, 0x25, 0x59, 0x9C, 0x19, 0x00};  // -> IAT 0xbeb390 SwitchToThread
    constexpr std::uintptr_t kIatSwitchToThread2021 = 0xBEB390;
    static_assert(kSysSleep2021 - (kLoop2021 + kRelOffset + 4) == 0x008C9DFE);  // the call's rel32 above

    // The Game Pass build (2021-04-13): the same code as Epic's at other addresses.
    constexpr std::uint32_t kTimeDateStamp2021GP = 0x6075C22A;
    constexpr std::uintptr_t kLoop2021GP = 0x17D825;
    constexpr std::uint8_t kLoop2021GPBytes[] = {
        0x0F, 0xB6, 0x44, 0x24, 0x60,  // movzx eax, byte [rsp+0x60]
        0x84, 0xC0,                    // test  al, al
        0x75, 0x1D,                    // jne   0x17d84b
        0x66, 0x90,                    // nop
        0x0F, 0xB6, 0x46, 0x10,        // movzx eax, byte [rsi+0x10]
        0x84, 0xC0,                    // test  al, al
        0x75, 0x13,                    // jne   0x17d84b
        0xB9, 0x01, 0x00, 0x00, 0x00,  // mov   ecx, 1
        0xE8, 0x0E, 0x07, 0x8E, 0x00,  // call  Sys_Sleep
        0x0F, 0xB6, 0x44, 0x24, 0x60,  // movzx eax, byte [rsp+0x60]
        0x84, 0xC0,                    // test  al, al
        0x74, 0xE5,                    // je    0x17d830
    };
    constexpr std::uintptr_t kSysSleep2021GP = 0xA5DF50;
    constexpr std::uint8_t kSysSleep2021GPBytes[] = {0x48, 0xFF, 0x25, 0x11, 0xB4, 0x1F, 0x00};  // -> IAT 0xc59368 Sleep
    constexpr std::uintptr_t kSysYield2021GP = 0xA68460;
    constexpr std::uint8_t kSysYield2021GPBytes[] = {0x48, 0xFF, 0x25, 0x59, 0x0F, 0x1F, 0x00};  // -> IAT 0xc593c0 SwitchToThread
    constexpr std::uintptr_t kIatSwitchToThread2021GP = 0xC593C0;
    static_assert(kSysSleep2021GP - (kLoop2021GP + kRelOffset + 4) == 0x008E070E);

    struct Build
    {
        std::uint32_t stamp;
        std::uintptr_t loop;
        std::span<const std::uint8_t> loopBytes;
        std::size_t relOffset;  // the rel32 of the call to Sys_Sleep, from loop
        std::uintptr_t sysSleep;
        std::span<const std::uint8_t> sysSleepBytes;
        std::uintptr_t sysYield;
        std::span<const std::uint8_t> sysYieldBytes;
        std::uintptr_t iatSwitchToThread;
        std::size_t doneFlag;      // the read's done flag, [rsp+doneFlag] in the loop's frame
        std::size_t shutdownFlag;  // StreamControl's "shutting down" byte, [rsi+shutdownFlag]
    };
    // The same two offsets in every build: the loop bytes above, which must match, contain both.
    constexpr Build kBuilds[] = {
        {kTimeDateStamp, kLoop, kLoopBytes, kRelOffset, kSysSleep, kSysSleepBytes, kSysYield, kSysYieldBytes,
         kIatSwitchToThread, kDoneFlag, kShutdownFlag},
        {kTimeDateStamp2021, kLoop2021, kLoop2021Bytes, kRelOffset, kSysSleep2021, kSysSleep2021Bytes, kSysYield2021,
         kSysYield2021Bytes, kIatSwitchToThread2021, kDoneFlag, kShutdownFlag},
        {kTimeDateStamp2021GP, kLoop2021GP, kLoop2021GPBytes, kRelOffset, kSysSleep2021GP, kSysSleep2021GPBytes,
         kSysYield2021GP, kSysYield2021GPBytes, kIatSwitchToThread2021GP, kDoneFlag, kShutdownFlag},
    };
    constexpr std::size_t kMaxLoop = 64;
    static_assert(sizeof(kLoopBytes) <= kMaxLoop && sizeof(kLoop2021Bytes) <= kMaxLoop &&
                  sizeof(kLoop2021GPBytes) <= kMaxLoop);

    WaitResult WaitForRead(const volatile std::uint8_t* done, const volatile std::uint8_t* shutdown,
                           std::int64_t yieldTicks, const WaitOps& ops)
    {
        WaitResult r;
        const std::int64_t start = ops.now();
        while (!*done && !*shutdown) {
            if (!r.slept && ops.now() - start < yieldTicks)
                ops.yield();
            else {
                r.slept = true;  // from here on it only sleeps, as the original did all along
                ops.sleep();
            }
        }
        r.done = *done != 0;
        r.ticks = ops.now() - start;
        return r;
    }

    namespace
    {
        // Set in Apply, before the call is retargeted.
        std::int64_t g_yieldTicks = 0;
        std::size_t g_doneFlag = kDoneFlag, g_shutdownFlag = kShutdownFlag;
        double g_ticksPerMs = 1.0;
        std::atomic<std::uint64_t> g_waits{0}, g_seenDone{0}, g_slept{0};
        std::atomic<std::int64_t> g_longest{0};  // ticks
        safetyhook::Allocation* g_jump = nullptr;  // never freed: the game may wait in it until exit

        std::int64_t Ticks()
        {
            LARGE_INTEGER c;
            QueryPerformanceCounter(&c);
            return c.QuadPart;
        }

        void YieldNow() { SwitchToThread(); }
        void SleepOneMs() { Sleep(1); }
        constexpr WaitOps kSystem{Ticks, YieldNow, SleepOneMs};

        void NoteLength(std::int64_t ticks)
        {
            std::int64_t seen = g_longest.load(std::memory_order_relaxed);
            while (ticks > seen && !g_longest.compare_exchange_weak(seen, ticks, std::memory_order_relaxed)) {}
        }

        // Called by the read-wait loop instead of Sys_Sleep(1) (ms is that 1, unused), through the
        // jump, which passes the loop's rsi as `control`. Returns once the read is done or
        // StreamControl is shutting down, so the loop leaves at its next check.
        __declspec(noinline) void SmartWait(unsigned /*ms*/, const volatile std::uint8_t* control)
        {
            const auto* frame = static_cast<const volatile std::uint8_t*>(_AddressOfReturnAddress()) + sizeof(void*);
            const WaitResult r = WaitForRead(frame + g_doneFlag, control + g_shutdownFlag, g_yieldTicks, kSystem);
            g_waits.fetch_add(1, std::memory_order_relaxed);
            if (r.done) g_seenDone.fetch_add(1, std::memory_order_relaxed);
            if (r.slept) g_slept.fetch_add(1, std::memory_order_relaxed);
            NoteLength(r.ticks);
        }

        // The jump's bytes: kJumpHead followed by SmartWait's address.
        void JumpBytes(std::uint8_t (&out)[kJumpSize])
        {
            const auto target = reinterpret_cast<std::uint64_t>(&SmartWait);
            std::memcpy(out, kJumpHead, sizeof(kJumpHead));
            std::memcpy(out + sizeof(kJumpHead), &target, sizeof(target));
        }
    }

    WaitStats TakeStats()
    {
        WaitStats s;
        s.waits = g_waits.exchange(0);
        s.seenDone = g_seenDone.exchange(0);
        s.slept = g_slept.exchange(0);
        s.longestMs = static_cast<double>(g_longest.exchange(0)) / g_ticksPerMs;
        return s;
    }

    static std::uint32_t TimeDateStamp(const std::uint8_t* base)
    {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        return nt->FileHeader.TimeDateStamp;
    }

    static std::string Hex(const std::uint8_t* p, std::size_t n)
    {
        std::string s;
        for (std::size_t i = 0; i < n; ++i) s += std::format("{:02x}", p[i]);
        return s;
    }

    const void* SmartWaitForTest() { return reinterpret_cast<const void*>(&SmartWait); }

    // Does the call already go to a SmartWait jump (a second copy of this plugin, or a second Apply)?
    // Only memory that can be read without a fault is looked at: no guard or no-access pages.
    static bool IsOurJump(const std::uint8_t* at)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(at, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
            !(mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_READONLY | PAGE_READWRITE)) ||
            (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            static_cast<const std::uint8_t*>(mbi.BaseAddress) + mbi.RegionSize < at + kJumpSize)
            return false;
        // This plugin's jump, or a plain jmp qword [rip+0]: some plugin's own wait either way.
        const std::uint8_t jmpRip[6] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
        return std::memcmp(at, kJumpHead, sizeof(kJumpHead)) == 0 || std::memcmp(at, jmpRip, sizeof(jmpRip)) == 0;
    }

    static const Build* FindBuild(std::uint32_t stamp)
    {
        for (const Build& b : kBuilds)
            if (b.stamp == stamp) return &b;
        return nullptr;
    }

    Result Apply(HMODULE exe, bool smartWait, int yieldUs)
    {
        auto* base = reinterpret_cast<std::uint8_t*>(exe);
        const std::uint32_t stamp = TimeDateStamp(base);
        const Build* b = FindBuild(stamp);
        if (!b) {
            LOG_INFO("Exe in memory at {}, PE timestamp 0x{:08x} (analysed builds: 0x{:08x}, 0x{:08x}, 0x{:08x})",
                     static_cast<void*>(base), stamp, kTimeDateStamp, kTimeDateStamp2021, kTimeDateStamp2021GP);
            LOG_WARN("Not one of the analysed builds of the game (was it updated?): changing nothing.");
            return Result::UnknownBuild;
        }
        LOG_INFO("Exe in memory at {}, PE timestamp 0x{:08x} (analysed build: 0x{:08x})", static_cast<void*>(base), stamp,
                 b->stamp);
        const std::uintptr_t call = b->loop + b->relOffset - 1;          // the call's e8
        const std::uintptr_t callNext = b->loop + b->relOffset + 4;      // where its rel32 counts from
        const auto relToSleep = static_cast<std::int32_t>(static_cast<std::intptr_t>(b->sysSleep - callNext));
        const auto relToYield = static_cast<std::int32_t>(static_cast<std::intptr_t>(b->sysYield - callNext));
        if (std::memcmp(base + b->sysSleep, b->sysSleepBytes.data(), b->sysSleepBytes.size()) != 0 ||
            std::memcmp(base + b->sysYield, b->sysYieldBytes.data(), b->sysYieldBytes.size()) != 0) {
            LOG_WARN("The Sleep / SwitchToThread thunks at 0x{:x} / 0x{:x} differ ({} / {}): changing nothing.",
                     b->sysSleep, b->sysYield, Hex(base + b->sysSleep, 7), Hex(base + b->sysYield, 7));
            return Result::UnknownBuild;
        }

        // The loop must match byte for byte, except the call's rel32, which may already be ours.
        const std::size_t n = b->loopBytes.size();
        std::array<std::uint8_t, kMaxLoop> loop{};
        std::memcpy(loop.data(), base + b->loop, n);
        std::int32_t rel = 0;
        std::memcpy(&rel, loop.data() + b->relOffset, sizeof(rel));
        std::memcpy(loop.data() + b->relOffset, b->loopBytes.data() + b->relOffset, sizeof(rel));
        if (std::memcmp(loop.data(), b->loopBytes.data(), n) != 0) {
            LOG_WARN("The read-wait loop at 0x{:x} differs from the analysed build ({}): changing nothing.", b->loop,
                     Hex(base + b->loop, n));
            return Result::UnknownBuild;
        }
        if (rel == relToYield) {
            LOG_INFO("The read-wait loop already calls SwitchToThread (a second copy of the plugin?): nothing to do.");
            return Result::AlreadyPatched;
        }
        if (rel != relToSleep && IsOurJump(base + callNext + rel)) {
            LOG_INFO("The read-wait loop already calls a SmartWait (a second copy of the plugin?): nothing to do.");
            return Result::AlreadyPatched;
        }
        if (rel != relToSleep) {
            LOG_WARN("The call at 0x{:x} targets 0x{:x}, not Sys_Sleep: changing nothing.", call,
                     callNext + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(rel)));
            return Result::UnknownBuild;
        }

        // Information only, not a gate: the thunk jumps through whatever this slot holds (the
        // offline harness maps the exe without resolving imports, and an overlay could hook it).
        void* slot = *reinterpret_cast<void**>(base + b->iatSwitchToThread);
        void* exported = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SwitchToThread"));
        LOG_INFO("SwitchToThread import slot holds {} (kernel32 exports {}){}", slot, exported,
                 slot == exported ? "" : "; differs, still fine: the thunk uses the slot");

        // SmartWait: the jump to the plugin's wait, near enough for the rel32. If there is no such
        // memory, plain FastReads still works.
        std::int32_t newRel = relToYield;
        const std::uint8_t* jump = nullptr;
        if (smartWait) {
            LARGE_INTEGER frequency;
            QueryPerformanceFrequency(&frequency);
            g_ticksPerMs = static_cast<double>(frequency.QuadPart) / 1000.0;
            g_yieldTicks = static_cast<std::int64_t>(std::max(0, yieldUs) / 1000.0 * g_ticksPerMs);
            g_doneFlag = b->doneFlag;
            g_shutdownFlag = b->shutdownFlag;
            auto allocation = safetyhook::Allocator::global()->allocate_near({base + callNext}, kJumpSize);
            const std::int64_t distance =
                allocation ? static_cast<std::int64_t>(allocation->address()) - static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(base + callNext))
                           : 0;
            if (!allocation || distance < INT32_MIN || distance > INT32_MAX) {
                LOG_WARN("SmartWait: no memory within reach of the game's code for its jump: plain FastReads (it always "
                         "yields) instead.");
            }
            else {
                std::uint8_t bytes[kJumpSize];
                JumpBytes(bytes);
                std::memcpy(allocation->data(), bytes, kJumpSize);
                FlushInstructionCache(GetCurrentProcess(), allocation->data(), kJumpSize);
                jump = allocation->data();
                newRel = static_cast<std::int32_t>(distance);
                g_jump = new safetyhook::Allocation(std::move(*allocation));
            }
        }

        std::uint8_t* target = base + b->loop + b->relOffset;
        DWORD oldProtect = 0;
        if (!VirtualProtect(target, sizeof(rel), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            LOG_ERROR("VirtualProtect failed (error {}): changing nothing.", GetLastError());
            return Result::Failed;
        }
        std::memcpy(target, &newRel, sizeof(newRel));
        DWORD unused = 0;
        if (!VirtualProtect(target, sizeof(rel), oldProtect, &unused))
            LOG_WARN("Could not restore the protection at 0x{:x} (error {}); the change itself is made.",
                     b->loop + b->relOffset, GetLastError());
        FlushInstructionCache(GetCurrentProcess(), target, sizeof(rel));

        if (std::memcmp(target, &newRel, sizeof(newRel)) != 0) {
            LOG_ERROR("Write did not stick at 0x{:x}.", b->loop + b->relOffset);
            return Result::Failed;
        }
        if (jump)
            LOG_INFO("Patched 0x{:x}: the read-completion poll in UncachedBackgroundRead now calls SmartWait (through a "
                     "jump at {}): it yields for up to {:.1f} ms of each read, then sleeps 1 ms at a time, instead of "
                     "always sleeping (Sleep(1)). Loop now: {}",
                     call, static_cast<const void*>(jump), std::max(0, yieldUs) / 1000.0, Hex(base + b->loop, n));
        else
            LOG_INFO("Patched 0x{:x}: the read-completion poll in UncachedBackgroundRead now yields (SwitchToThread) "
                     "instead of sleeping (Sleep(1)). Loop now: {}",
                     call, Hex(base + b->loop, n));
        return Result::Patched;
    }
}
