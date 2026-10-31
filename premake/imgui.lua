project "ImGui"
    location "../build/ImGui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    includedirs { "../external/imgui", "../external/glfw/include" }
    defines { "GLFW_INCLUDE_NONE" }
    files {
        "../external/imgui/imgui.h", "../external/imgui/imconfig.h",
        "../external/imgui/imgui.cpp", "../external/imgui/imgui_draw.cpp",
        "../external/imgui/imgui_tables.cpp", "../external/imgui/imgui_widgets.cpp",
        "../external/imgui/backends/imgui_impl_glfw.h",
        "../external/imgui/backends/imgui_impl_glfw.cpp"
    }
