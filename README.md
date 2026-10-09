# Wolfenstein Fast Loading

Source code of two plugins that shorten the loading in MachineGames' Wolfenstein games. Download,
installation and support are on Nexus Mods:

- **Wolfenstein The New Order - Fast Loading** (`TNOFastLoading.asi`):
  https://www.nexusmods.com/wolfensteintheneworder/mods/22
- **Wolfenstein The Old Blood - Fast Loading** (`TOBFastLoading.asi`): the Nexus page is not up yet.

The two share their loading modules (`src/*.cpp`); each has its own DllMain, switches and version
information, in `src/loadfix` (The New Order) and `src/tobfix` (The Old Blood). `src/quit_fix.cpp`
is The Old Blood only. The ini files are in `config/`.

Building needs Visual Studio 2026 (MSVC) and xmake:

    git clone --recurse-submodules https://github.com/Deepo/WolfensteinFastLoading.git
    cd WolfensteinFastLoading
    xmake f -p windows -a x64 -m release -y
    xmake -y

The plugins: build\windows\x64\release\TNOFastLoading.asi and TOBFastLoading.asi. `xmake -y
TNOFastLoading` or `xmake -y TOBFastLoading` builds just one.

MIT License. Uses SafetyHook (BSL-1.0), Zydis and Zycore (MIT) and mINI (MIT), as git
submodules in external/. The version information and log layout follow Lyall's game fixes
(MIT): see THIRD-PARTY-NOTICES.txt.
