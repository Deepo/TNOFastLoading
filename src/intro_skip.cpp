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
//
// The Old Blood: the initialiser at 0xb5c990 has the 2014 shape (the lea at 0xb5c99b), but no "1"
// follows its "0" (0xe44944), so the default points at the "1" just before "skips the intro
// video", com_waitForSavegames' default (0xe44a1c). idCommonLocal::Init reads the cvar the same
// way (0x12e5da). Its Init2 (0x1650b0) differs: with the cvar set it still plays the video during
// "common" while the cvar bink_dontfree is set (its default), but marks it to end with the load
// and doesn't wait for it, so the logo would show while "common" loads and stop when it's done:
//
//   0x16512a  lea   rbx, [rip+...]          ; "bink/loadvideo_pc_xboxone.bik"
//   0x165131  xor   r14d, r14d
//   0x165134  test  sil, sil                ; skip?
//   0x165137  je    0x165144
//   0x165139  cmp   [bink_dontfree], r14d
//   0x165140  cmove rbx, r14                ; no video only if bink_dontfree is 0
//
// With bink_dontfree the render system keeps the Bink textures of the first video it plays, and
// refuses any later video of other dimensions ("does not match dimensions of previously played
// video"); the logo makes that first video a 1920x1080 one. Every video The Old Blood ships is
// 1920x1080, so here SkipIntroVideo also makes the cmove a plain `mov rbx, r14` (and a nop): with
// the cvar set there is no video at all, as in The New Order, and the first loading video creates
// the textures instead.
//
// The Old Blood also has warning screens before its main menu (a photosensitivity warning and an
// auto-save notice; TNO has none). Its own switch is the cvar skipInitialWarningScreens ("Skip
// initial warning screens such as photo sensitivity warning and auto-save notice", default "0"),
// read only by the session's state machine (0x4f9b13): while no player has signed in, it moves the
// session from its first state (0, "press start") to the next at once. ApplyWarningScreens makes
// it start as 1 the same way: its initialiser at 0xb854a0 has the 2014 shape, and the default
// points at net_forceMatchBrowser's "1" (0xef9444).
//
// The Old Blood's Game Pass build is a 2021 rebuild like The New Order's: both initialisers have
// Epic's shape (com_skipIntroVideo at 0x27370, skipInitialWarningScreens at 0x351f0), with the
// merged "0" (0xc87a28) and "1" (0xc88198); Init2's choice of the video is the same code at
// 0x174689.
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

    // The Old Blood (GOG): the 2014 shape.
    constexpr std::uint32_t kTimeDateStampTOB = 0x554C7C23;
    constexpr std::uintptr_t kInitTOB = 0xB5C990;
    constexpr std::uint8_t kInitTOBBytes[] = {
        0x48, 0x83, 0xEC, 0x38,                                // sub  rsp, 0x38
        0x48, 0x8D, 0x05, 0x85, 0x80, 0x2E, 0x00,              // lea  rax, [rip+0x2e8085]  ("skips the intro video")
        0x4C, 0x8D, 0x05, 0xA2, 0x7F, 0x2E, 0x00,              // lea  r8,  [rip+0x2e7fa2]  ("0")
        0x48, 0x8D, 0x15, 0x8F, 0x80, 0x2E, 0x00,              // lea  rdx, [rip+0x2e808f]  ("com_skipIntroVideo")
        0x48, 0x8D, 0x0D, 0xE0, 0x68, 0xCC, 0x00,              // lea  rcx, [rip+0xcc68e0]  (the cvar)
        0x41, 0xB9, 0x01, 0x00, 0x00, 0x00,                    // mov  r9d, 1
        0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00,  // mov  qword ptr [rsp+0x28], 0
        0x48, 0x89, 0x44, 0x24, 0x20,                          // mov  qword ptr [rsp+0x20], rax
        0xE8, 0x07, 0x88, 0xE5, 0xFF,                          // call 0x9b51d0
        0x48, 0x8D, 0x0D, 0x30, 0x30, 0x05, 0x00,              // lea  rcx, [rip+0x53030]
        0x48, 0x83, 0xC4, 0x38,                                // add  rsp, 0x38
        0xE9, 0xCF, 0x8F, 0xF5, 0xFF,                          // jmp  0xab59a8
    };
    constexpr std::uintptr_t kZeroTOB = 0xE44944;  // "0"
    constexpr std::uintptr_t kOneTOB = 0xE44A1C;   // "1" (com_waitForSavegames' default)
    constexpr std::uintptr_t kNameTOB = 0xE44A38;  // "com_skipIntroVideo"
    constexpr std::uintptr_t kCVarTOB = 0x1823290;
    static_assert(kZeroTOB - (kInitTOB + kDispOffset + 4) == 0x2E7FA2);  // the lea's disp above
    constexpr std::uintptr_t kLogoTOB = 0x16512A;  // Init2's choice of the video, above
    constexpr std::uint8_t kLogoTOBBytes[] = {
        0x48, 0x8D, 0x1D, 0xDF, 0x03, 0xCF, 0x00,  // lea   rbx, [rip+0xcf03df]  ("bink/loadvideo_pc_xboxone.bik")
        0x45, 0x33, 0xF6,                          // xor   r14d, r14d
        0x40, 0x84, 0xF6,                          // test  sil, sil
        0x74, 0x0B,                                // je    0x165144
        0x44, 0x39, 0x35, 0x18, 0xFD, 0xDB, 0x01,  // cmp   [bink_dontfree], r14d
        0x49, 0x0F, 0x44, 0xDE,                    // cmove rbx, r14
    };
    constexpr std::size_t kLogoOffsetTOB = 0x16;                         // the cmove, at 0x165140
    constexpr std::uint8_t kNoLogo[] = {0x49, 0x8B, 0xDE, 0x90};       // mov rbx, r14; nop
    static_assert(sizeof(kLogoTOBBytes) == kLogoOffsetTOB + sizeof(kNoLogo));

    // The Old Blood, Game Pass (2021-04-14): Epic's shape.
    constexpr std::uint32_t kTimeDateStampTOBGP = 0x60770B00;
    constexpr std::uintptr_t kInitTOBGP = 0x27370;
    constexpr std::uint8_t kInitTOBGPBytes[] = {
        0x48, 0x83, 0xEC, 0x38,                                // sub  rsp, 0x38
        0x48, 0x8D, 0x05, 0xDD, 0x3C, 0xC7, 0x00,              // lea  rax, [rip+0xc73cdd]  ("skips the intro video")
        0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00,  // mov  qword ptr [rsp+0x28], 0
        0x41, 0xB9, 0x01, 0x00, 0x00, 0x00,                    // mov  r9d, 1
        0x48, 0x89, 0x44, 0x24, 0x20,                          // mov  qword ptr [rsp+0x20], rax
        0x4C, 0x8D, 0x05, 0x92, 0x06, 0xC6, 0x00,              // lea  r8,  [rip+0xc60692]  ("0")
        0x48, 0x8D, 0x15, 0xD3, 0x3C, 0xC7, 0x00,              // lea  rdx, [rip+0xc73cd3]  ("com_skipIntroVideo")
        0x48, 0x8D, 0x0D, 0xDC, 0x81, 0x55, 0x01,              // lea  rcx, [rip+0x15581dc] (the cvar)
        0xE8, 0x27, 0x12, 0xA4, 0x00,                          // call 0xa685d0
        0x48, 0x8D, 0x0D, 0xA0, 0xB7, 0xC3, 0x00,              // lea  rcx, [rip+0xc3b7a0]
        0x48, 0x83, 0xC4, 0x38,                                // add  rsp, 0x38
        0xE9, 0x97, 0xC5, 0xBE, 0x00,                          // jmp  0xc13950
    };
    constexpr std::uintptr_t kZeroTOBGP = 0xC87A28;  // "0"
    constexpr std::uintptr_t kOneTOBGP = 0xC88198;   // "1"
    constexpr std::uintptr_t kNameTOBGP = 0xC9B070;  // "com_skipIntroVideo"
    constexpr std::uintptr_t kCVarTOBGP = 0x157F580;
    static_assert(kZeroTOBGP - (kInitTOBGP + kDispOffset2021 + 4) == 0xC60692);
    constexpr std::uintptr_t kLogoTOBGP = 0x174689;
    constexpr std::uint8_t kLogoTOBGPBytes[] = {
        0x48, 0x8D, 0x1D, 0xF8, 0x69, 0xB3, 0x00,  // lea   rbx, [rip+0xb369f8]  ("bink/loadvideo_pc_xboxone.bik")
        0x45, 0x33, 0xF6,                          // xor   r14d, r14d
        0x40, 0x84, 0xF6,                          // test  sil, sil
        0x74, 0x0B,                                // je    0x1746a3
        0x44, 0x39, 0x35, 0xE9, 0x7A, 0xB2, 0x01,  // cmp   [bink_dontfree], r14d
        0x49, 0x0F, 0x44, 0xDE,                    // cmove rbx, r14
    };
    static_assert(sizeof(kLogoTOBGPBytes) == kLogoOffsetTOB + sizeof(kNoLogo));

    struct Build
    {
        std::uint32_t stamp;
        std::uintptr_t init;
        std::span<const std::uint8_t> initBytes;
        std::size_t dispOffset;
        std::uintptr_t zero, one, name;  // the strings "0", "1", "com_skipIntroVideo"
        std::uintptr_t cvar;
        // The Old Blood's Init2: the code that chooses the logo video, and where kNoLogo goes.
        std::uintptr_t logo = 0;
        std::span<const std::uint8_t> logoBytes = {};
        std::size_t logoOffset = 0;
    };
    constexpr Build kBuilds[] = {
        {kTimeDateStamp, kInit, kInitBytes, kDispOffset, kZero, kOne, kName, kCVar},
        {kTimeDateStamp2021, kInit2021, kInit2021Bytes, kDispOffset2021, kZero2021, kOne2021, kName2021, kCVar2021},
        {kTimeDateStamp2021GP, kInit2021GP, kInit2021GPBytes, kDispOffset2021, kZero2021GP, kOne2021GP, kName2021GP,
         kCVar2021GP},
        {kTimeDateStampTOB, kInitTOB, kInitTOBBytes, kDispOffset, kZeroTOB, kOneTOB, kNameTOB, kCVarTOB, kLogoTOB,
         kLogoTOBBytes, kLogoOffsetTOB},
        {kTimeDateStampTOBGP, kInitTOBGP, kInitTOBGPBytes, kDispOffset2021, kZeroTOBGP, kOneTOBGP, kNameTOBGP, kCVarTOBGP,
         kLogoTOBGP, kLogoTOBGPBytes, kLogoOffsetTOB},
    };
    constexpr std::size_t kMaxInit = 96;
    static_assert(sizeof(kInitBytes) <= kMaxInit && sizeof(kInit2021Bytes) <= kMaxInit &&
                  sizeof(kInit2021GPBytes) <= kMaxInit && sizeof(kInitTOBBytes) <= kMaxInit &&
                  sizeof(kInitTOBGPBytes) <= kMaxInit);

    // The Old Blood's skipInitialWarningScreens (ApplyWarningScreens): the 2014 shape.
    constexpr std::uintptr_t kWarnInitTOB = 0xB854A0;
    constexpr std::uint8_t kWarnInitTOBBytes[] = {
        0x48, 0x83, 0xEC, 0x38,                                // sub  rsp, 0x38
        0x48, 0x8D, 0x05, 0xA5, 0x53, 0x37, 0x00,              // lea  rax, [rip+0x3753a5]  ("Skip initial warning screens ...")
        0x4C, 0x8D, 0x05, 0x12, 0x40, 0x37, 0x00,              // lea  r8,  [rip+0x374012]  ("0")
        0x48, 0x8D, 0x15, 0xEF, 0x53, 0x37, 0x00,              // lea  rdx, [rip+0x3753ef]  ("skipInitialWarningScreens")
        0x48, 0x8D, 0x0D, 0x70, 0x3F, 0x3E, 0x01,              // lea  rcx, [rip+0x13e3f70] (the cvar)
        0x41, 0xB9, 0x01, 0x00, 0x00, 0x00,                    // mov  r9d, 1
        0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00,  // mov  qword ptr [rsp+0x28], 0
        0x48, 0x89, 0x44, 0x24, 0x20,                          // mov  qword ptr [rsp+0x20], rax
        0xE8, 0xF7, 0xFC, 0xE2, 0xFF,                          // call 0x9b51d0
        0x48, 0x8D, 0x0D, 0x50, 0x12, 0x03, 0x00,              // lea  rcx, [rip+0x31250]
        0x48, 0x83, 0xC4, 0x38,                                // add  rsp, 0x38
        0xE9, 0xBF, 0x04, 0xF3, 0xFF,                          // jmp  0xab59a8
    };
    constexpr std::uintptr_t kWarnZeroTOB = 0xEF94C4;  // "0"
    constexpr std::uintptr_t kWarnOneTOB = 0xEF9444;   // "1" (net_forceMatchBrowser's default)
    constexpr std::uintptr_t kWarnNameTOB = 0xEFA8A8;  // "skipInitialWarningScreens"
    constexpr std::uintptr_t kWarnCVarTOB = 0x1F69430;
    static_assert(kWarnZeroTOB - (kWarnInitTOB + kDispOffset + 4) == 0x374012);  // the lea's disp above
    // The Old Blood, Game Pass: Epic's shape, the merged "0" and "1".
    constexpr std::uintptr_t kWarnInitTOBGP = 0x351F0;
    constexpr std::uint8_t kWarnInitTOBGPBytes[] = {
        0x48, 0x83, 0xEC, 0x38,                                // sub  rsp, 0x38
        0x48, 0x8D, 0x05, 0x65, 0x6B, 0xCB, 0x00,              // lea  rax, [rip+0xcb6b65]  ("Skip initial warning screens ...")
        0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00,  // mov  qword ptr [rsp+0x28], 0
        0x41, 0xB9, 0x01, 0x00, 0x00, 0x00,                    // mov  r9d, 1
        0x48, 0x89, 0x44, 0x24, 0x20,                          // mov  qword ptr [rsp+0x20], rax
        0x4C, 0x8D, 0x05, 0x12, 0x28, 0xC5, 0x00,              // lea  r8,  [rip+0xc52812]  ("0")
        0x48, 0x8D, 0x15, 0x9B, 0x6B, 0xCB, 0x00,              // lea  rdx, [rip+0xcb6b9b]  ("skipInitialWarningScreens")
        0x48, 0x8D, 0x0D, 0xDC, 0x80, 0xC3, 0x01,              // lea  rcx, [rip+0x1c380dc] (the cvar)
        0xE8, 0xA7, 0x33, 0xA3, 0x00,                          // call 0xa685d0
        0x48, 0x8D, 0x0D, 0x90, 0x13, 0xC3, 0x00,              // lea  rcx, [rip+0xc31390]
        0x48, 0x83, 0xC4, 0x38,                                // add  rsp, 0x38
        0xE9, 0x17, 0xE7, 0xBD, 0x00,                          // jmp  0xc13950
    };
    constexpr std::uintptr_t kWarnNameTOBGP = 0xCEBDB8;  // "skipInitialWarningScreens"
    constexpr std::uintptr_t kWarnCVarTOBGP = 0x1C6D300;
    static_assert(kZeroTOBGP - (kWarnInitTOBGP + kDispOffset2021 + 4) == 0xC52812);
    constexpr Build kWarningBuilds[] = {
        {kTimeDateStampTOB, kWarnInitTOB, kWarnInitTOBBytes, kDispOffset, kWarnZeroTOB, kWarnOneTOB, kWarnNameTOB,
         kWarnCVarTOB},
        {kTimeDateStampTOBGP, kWarnInitTOBGP, kWarnInitTOBGPBytes, kDispOffset2021, kZeroTOBGP, kOneTOBGP, kWarnNameTOBGP,
         kWarnCVarTOBGP},
    };
    static_assert(sizeof(kWarnInitTOBBytes) <= kMaxInit && sizeof(kWarnInitTOBGPBytes) <= kMaxInit);

    static std::string Hex(const std::uint8_t* p, std::size_t n)
    {
        std::string s;
        for (std::size_t i = 0; i < n; ++i) s += std::format("{:02x}", p[i]);
        return s;
    }

    // Writes n bytes of code; false (and the log says why) if it didn't stick.
    static bool WriteCode(const char* what, std::uint8_t* at, const void* bytes, std::size_t n)
    {
        DWORD oldProtect = 0;
        if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            LOG_ERROR("{}: VirtualProtect failed (error {}): changing nothing.", what, GetLastError());
            return false;
        }
        std::memcpy(at, bytes, n);
        DWORD unused = 0;
        if (!VirtualProtect(at, n, oldProtect, &unused))
            LOG_WARN("{}: could not restore the protection at {} (error {}); the change itself is made.", what,
                     static_cast<void*>(at), GetLastError());
        FlushInstructionCache(GetCurrentProcess(), at, n);
        if (std::memcmp(at, bytes, n) != 0) {
            LOG_ERROR("{}: write did not stick at {}.", what, static_cast<void*>(at));
            return false;
        }
        return true;
    }

    // One cvar whose default this module changes from "0" to "1".
    struct Target
    {
        std::span<const Build> builds;
        const char* cvar;    // its name, as the exe stores it
        const char* what;    // the log lines' prefix
        const char* effect;  // what it does, for the log
    };

    static Result ApplyTo(HMODULE exe, const Target& t)
    {
        auto* base = reinterpret_cast<std::uint8_t*>(exe);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        const Build* b = nullptr;
        for (const Build& candidate : t.builds)
            if (candidate.stamp == nt->FileHeader.TimeDateStamp) b = &candidate;
        if (!b) {
            LOG_WARN("{}: not the analysed build (PE timestamp 0x{:08x}): changing nothing.", t.what,
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
            std::memcmp(base + b->one, "1", 2) != 0 || std::memcmp(base + b->name, t.cvar, std::strlen(t.cvar) + 1) != 0) {
            LOG_WARN("{}: the initialiser of {} at 0x{:x} differs from the analysed build ({}): changing nothing.", t.what,
                     t.cvar, b->init, Hex(base + b->init, n));
            return Result::UnknownBuild;
        }
        if (disp == patchedDisp) {
            LOG_INFO("{}: {} already defaults to 1: nothing to do.", t.what, t.cvar);
            return Result::AlreadyPatched;
        }
        if (disp != originalDisp) {
            LOG_WARN("{}: the default of {} points at 0x{:x}, not 0x{:x}: changing nothing.", t.what, t.cvar,
                     dispEnd + disp, b->zero);
            return Result::UnknownBuild;
        }
        // The Old Blood: Init2's choice of the logo video must be the analysed code.
        if (b->logo && std::memcmp(base + b->logo, b->logoBytes.data(), b->logoBytes.size()) != 0) {
            LOG_WARN("{}: the logo video's choice in Init2 at 0x{:x} differs from the analysed build ({}): changing "
                     "nothing.", t.what, b->logo, Hex(base + b->logo, b->logoBytes.size()));
            return Result::UnknownBuild;
        }

        // Not built yet? Then its memory (.bss) is still all zero.
        for (std::size_t i = 0; i < kCVarSize; ++i) {
            if (base[b->cvar + i] != 0) {
                LOG_WARN("{}: {} already exists (the plugin was loaded after the game's start-up code), so changing its "
                         "default would do nothing: changing nothing.", t.what, t.cvar);
                return Result::Failed;
            }
        }

        if (!WriteCode(t.what, base + b->init + b->dispOffset, &patchedDisp, sizeof(patchedDisp))) return Result::Failed;
        LOG_INFO("Patched 0x{:x}: {} starts as 1 instead of 0, as with +{} 1 ({}).", b->init + b->dispOffset, t.cvar,
                 t.cvar, t.effect);
        if (b->logo) {
            if (!WriteCode(t.what, base + b->logo + b->logoOffset, kNoLogo, sizeof(kNoLogo))) {
                // All or nothing: the default goes back to "0".
                WriteCode(t.what, base + b->init + b->dispOffset, &originalDisp, sizeof(originalDisp));
                return Result::Failed;
            }
            LOG_INFO("Patched 0x{:x}: with {} set, Init2 plays no logo video at all (it played one while \"common\" "
                     "loaded whenever bink_dontfree is set).", b->logo + b->logoOffset, t.cvar);
        }
        return Result::Patched;
    }

    Result Apply(HMODULE exe)
    {
        return ApplyTo(exe, {kBuilds, "com_skipIntroVideo", "Intro video", "the logo video no longer holds the start-up"});
    }

    Result ApplyWarningScreens(HMODULE exe)
    {
        return ApplyTo(exe, {kWarningBuilds, "skipInitialWarningScreens", "Warning screens",
                             "the warning screens before the main menu are skipped"});
    }
}
