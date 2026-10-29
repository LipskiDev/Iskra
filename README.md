# Iskra

Windows x64 C++20 project built with Premake, following Iryven's separate dependency scripts and output directories.

- `Iskra/`: application headers and source files together.
- `premake/`: GLFW and NVRHI build definitions.
- `external/`: dependency source checkouts, including their upstream licenses.
- `build/`: generated Visual Studio projects.
- `bin/<configuration>-windows-x86_64/`: binaries and static libraries.
- `bin-int/<configuration>-windows-x86_64/`: intermediate files.

## Build

Install Visual Studio 2022 (Desktop development with C++ and a Windows SDK) and Premake 5 (tested with 5.0.0-beta8). Run `GenerateProjects.bat`, then open `Iskra.sln`. The script accepts a Premake executable path as its first argument; otherwise it looks in `tools/`, `C:\dev\tools`, and PATH.

Configurations: Debug, Release, and Profile (optimized with symbols). All use the dynamic MSVC runtime. GLFW and NVRHI build from source as static libraries. NVRHI includes its validation layer and D3D12 backend. Vulkan, D3D11, NVAPI, RTXMU, and Aftermath are not enabled.

## Dependencies

The source checkouts are already present. To restore them in a fresh copy, run `powershell -ExecutionPolicy Bypass -File tools/FetchDependencies.ps1` before generating projects. The script pins exact revisions and does not overwrite existing checkouts. Keep the upstream license files when redistributing.

| Dependency | Revision |
| --- | --- |
| GLFW 3.4 | `7b6aead9fb88b3623e3b3725ebb42670cbe4c579` |
| NVRHI | `bf0bbd9b666a7297492beb1999a4ddc0dfded143` |
| DirectX-Headers (required by NVRHI D3D12) | `d873b344dc540898868697245f100c2a67fe68d9` |

The original CMake entry points have been replaced by Premake. Existing `out/` and `.vs/` caches are ignored.
