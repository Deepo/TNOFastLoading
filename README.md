# TNOFastLoading

Source code of Wolfenstein The New Order - Fast Loading. Download, installation and support:
https://www.nexusmods.com/wolfensteintheneworder/mods/22

Building needs Visual Studio 2026 (MSVC) and xmake:

    git clone --recurse-submodules https://github.com/Deepo/TNOFastLoading.git
    cd TNOFastLoading
    xmake f -p windows -a x64 -m release -y
    xmake -y

The plugin: build\windows\x64\release\TNOFastLoading.asi

MIT License. Uses SafetyHook (BSL-1.0), Zydis and Zycore (MIT) and mINI (MIT), as git
submodules in external/. The version information and log layout follow Lyall's game fixes
(MIT): see THIRD-PARTY-NOTICES.txt.
