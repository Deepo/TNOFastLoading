local name = "TNOFastLoading"

set_project(name)
add_rules("mode.debug", "mode.release")
set_languages("cxxlatest", "clatest")

-- ===================== Zycore / Zydis (for safetyhook) =====================
-- Git submodules at pinned commits.

target("zycore")
    set_kind("static")
    add_defines("ZYCORE_STATIC_BUILD", {public = true})
    add_includedirs("external/zydis/dependencies/zycore/include", {public = true})
    add_files("external/zydis/dependencies/zycore/src/*.c")
    set_runtimes(is_mode("release") and "MT" or "MTd")

target("zydis")
    add_deps("zycore")
    set_kind("static")
    add_defines("ZYDIS_STATIC_BUILD", {public = true})
    add_includedirs("external/zydis/include", "external/zydis/src", {public = true})
    add_files("external/zydis/src/*.c")
    set_runtimes(is_mode("release") and "MT" or "MTd")

-- ===================== TNOFastLoading.asi =====================
-- "Wolfenstein The New Order - Fast Loading" by Deepo on Nexus Mods.
-- src/loadfix (DllMain, its switches, its version info) and the four loading modules. Loaded by
-- Ultimate ASI Loader as dinput8.dll. Byte patches in DllMain (read_wait_fix, preload_wait_fix,
-- intro_skip), SmartWait's jump near the exe, and one safetyhook mid-function hook in the load
-- screen's wait loop (load_prompt); each switchable in TNOFastLoading.ini.

target(name)
    set_kind("shared")
    set_prefixname("")
    set_extension(".asi")

    add_includedirs("src/loadfix", "src", "external/safetyhook/include", "external/mINI/src/mini")
    add_headerfiles("src/loadfix/*.h", "src/*.h")
    add_files("src/loadfix/*.cpp", "src/loadfix/version.rc",
              "src/read_wait_fix.cpp", "src/preload_wait_fix.cpp", "src/load_prompt.cpp", "src/intro_skip.cpp",
              "src/patch_result.cpp", "src/config.cpp", "src/logger.cpp", "external/safetyhook/src/**.cpp")
    add_deps("zydis")

    if is_plat("windows") then
        set_toolchains("msvc")
        add_cxflags("/utf-8", "/W4")
        if is_mode("release") then
            set_optimize("fastest")
            set_strip("all")
            set_runtimes("MT")
            add_ldflags("/OPT:REF", "/OPT:ICF")
        else
            set_strip("none")
            set_runtimes("MTd")
            add_cxflags("/Zi")
            add_defines("_DEBUG")
        end
    end
