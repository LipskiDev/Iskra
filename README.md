# Iskra

A GPU research environment for implementing and exploring scientific papers in rendering, simulation, and beyond.

Built with C++20, GLFW, GLM, Assimp, and NVRHI using Direct3D 12 on Windows x64.

## Build

Requires Visual Studio with Desktop development with C++, a Windows SDK, Premake 5, and CMake 3.22+ on PATH.

1. If dependencies are missing, run `powershell -ExecutionPolicy Bypass -File tools/FetchDependencies.ps1`.
2. Run `GenerateProjects.bat` with Premake available on PATH, or pass its executable path as the first argument.
3. Open `Iskra.sln` and build Debug, Release, or Profile.

Assimp v6.0.5 is included as a submodule and built automatically with CMake. Its DLL is placed beside the application.
