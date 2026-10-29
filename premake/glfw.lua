project "GLFW"
    location "../build/GLFW"
    kind "StaticLib"
    language "C"
    cdialect "C99"
    includedirs { "../external/glfw/include", "../external/glfw/src" }
    defines { "_GLFW_WIN32", "_CRT_SECURE_NO_WARNINGS" }
    files {
        "../external/glfw/include/GLFW/*.h", "../external/glfw/src/*.h",
        "../external/glfw/src/context.c", "../external/glfw/src/init.c",
        "../external/glfw/src/input.c", "../external/glfw/src/monitor.c",
        "../external/glfw/src/platform.c", "../external/glfw/src/vulkan.c",
        "../external/glfw/src/window.c", "../external/glfw/src/egl_context.c",
        "../external/glfw/src/osmesa_context.c", "../external/glfw/src/null_*.c",
        "../external/glfw/src/win32_*.c", "../external/glfw/src/wgl_context.c"
    }
