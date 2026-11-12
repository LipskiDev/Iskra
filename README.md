# Iskra

A GPU research environment for implementing and exploring scientific papers in rendering, simulation, and beyond.

Built with C++20, GLFW, GLM, Assimp, and NVRHI using Direct3D 12 on Windows x64.

## Build

Requires Visual Studio with Desktop development with C++, a Windows SDK, Premake 5, Python 3 (`python`), and CMake 3.22+ on PATH.

1. If dependencies are missing, run `powershell -ExecutionPolicy Bypass -File tools/FetchDependencies.ps1`.
2. Run `GenerateProjects.bat` with Premake available on PATH, or pass its executable path as the first argument.
3. Open `Iskra.sln` and build Debug, Release, or Profile.

Assimp v6.0.5 is included as a submodule and built automatically with CMake. Its DLL is placed beside the application.

Shaders compile automatically through `tools/CompileShader.py` with DXC (`dxc.exe` on PATH). Declare stages on the first line, for example `//@stages vs ps` or `//@stages ms ps`. Supported stages are `vs`, `ps`, `gs`, `hs`, `ds`, `cs`, `ms`, and `as`, with entry points `VSMain`, `PSMain`, etc., targeting shader model 6.5. Each stage produces `<filename>.<stage>.dxil` in the application's `shaders/` directory for each configuration. Debug embeds shader debug information and disables optimization; Release and Profile use `-O3`. Shared `.hlsli` changes also trigger compilation. Files without a stage declaration are not compiled. Regenerate projects after adding shaders or changing stage declarations so MSBuild tracks the correct outputs.

The ImGui overlay uses GLFW for input and NVRHI for rendering. The Lighting panel controls sun azimuth and elevation; interacting with the UI captures camera input. Docking and additional OS windows are not enabled.
