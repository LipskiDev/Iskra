# Iskra

A GPU research environment for implementing and exploring scientific papers in rendering, simulation, and beyond.

Built with C++20, GLFW, and NVRHI using Direct3D 12 on Windows x64.

## Build

Requires Visual Studio 2022 with Desktop development with C++, a Windows SDK, and Premake 5.

1. If dependencies are missing, run `powershell -ExecutionPolicy Bypass -File tools/FetchDependencies.ps1`.
2. Run `GenerateProjects.bat` with Premake available on PATH, or pass its executable path as the first argument.
3. Open `Iskra.sln` and build Debug, Release, or Profile.
