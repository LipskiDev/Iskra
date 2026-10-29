workspace "Iskra"
    architecture "x86_64"
    configurations { "Debug", "Release", "Profile" }
    startproject "Iskra"
    system "windows"
    systemversion "latest"
    multiprocessorcompile "On"
    staticruntime "Off"
    targetdir "bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}"
    objdir "bin-int/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}"
    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"
    filter "configurations:Release"
        runtime "Release"
        optimize "Speed"
    filter "configurations:Profile"
        runtime "Release"
        optimize "Speed"
        symbols "On"
    filter {}
group "Dependencies"
include "premake/glfw.lua"
include "premake/nvrhi.lua"
group ""
project "Iskra"
    location "build/Iskra"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    debugdir (_MAIN_SCRIPT_DIR)
    files { "Iskra/**.h", "Iskra/**.cpp", "Iskra/**.hlsl", "Iskra/**.hlsli" }
    vpaths {
        ["Shaders"] = { "Iskra/**.hlsl", "Iskra/**.hlsli" }
    }
    filter "files:**.hlsl"
        buildaction "None"
    filter "files:**.hlsli"
        buildaction "None"
    filter {}
    includedirs { "Iskra", "external/glfw/include", "external/nvrhi/include", "external/DirectX-Headers/include" }
    defines { "GLFW_INCLUDE_NONE", "NOMINMAX", "NVRHI_D3D12_WITH_DXRCLUSTER=0" }
    links { "GLFW", "NVRHI", "d3d12", "dxgi", "dxguid", "user32", "gdi32", "shell32" }
