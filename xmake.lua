add_rules("mode.debug", "mode.release")

if is_plat("windows") then
    if not has_config("vs_runtime") then
        set_runtimes("MD")
    end
else
    set_toolchains("clang")
end

target("MotdPE")
    set_kind("static")
    set_languages("c++23")
    set_exceptions("none")
    add_includedirs("include")
    add_files("src/**.cpp")
    if is_mode("debug") then
        set_symbols("debug")
    else
        set_optimize("aggressive")
        set_strip("all")
    end
    
    if is_plat("windows") then
        add_defines(
            "NOMINMAX",
            "UNICODE"
        )
        add_cxflags(
            "/EHsc",
            "/utf-8",
            "/W4"
        )
        add_syslinks("ws2_32")
        if is_mode("release") then
            add_cxflags(
                "/O2",
                "/Ob3"
            )
        end
    else
        add_cxxflags("-Wno-gnu-line-marker")
        add_cxflags(
            "-Wall",
            "-pedantic",
            "-fexceptions",
            "-stdlib=libc++",
            "-fPIC"
        )
        add_ldflags(
            "-stdlib=libc++"
        )
        if is_mode("release") then
            add_cxflags(
                "-O3"
            )
        end
    end