param(
    [ValidateSet('Debug', 'Release', 'Profile')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$sourceDir = Join-Path $projectRoot 'external/assimp'
$buildDir = Join-Path $projectRoot "build/assimp-cmake/$Configuration"
$libraryDir = Join-Path $projectRoot "bin/$Configuration-windows-x86_64/Assimp"
$runtimeDir = Join-Path $projectRoot "bin/$Configuration-windows-x86_64/Iskra"
$cmakeConfig = if ($Configuration -eq 'Profile') { 'RelWithDebInfo' } else { $Configuration }
$outputConfig = $cmakeConfig.ToUpperInvariant()

if (!(Test-Path "$sourceDir/CMakeLists.txt")) {
    throw 'Assimp is missing. Run git submodule update --init --recursive.'
}

# Use the upstream build so its importer list and bundled dependencies stay intact.
& cmake -S $sourceDir -B $buildDir -A x64 `
    '-DBUILD_SHARED_LIBS=ON' `
    '-DASSIMP_BUILD_TESTS=OFF' `
    '-DASSIMP_BUILD_ASSIMP_TOOLS=OFF' `
    '-DASSIMP_BUILD_SAMPLES=OFF' `
    '-DASSIMP_BUILD_ZLIB=ON' `
    '-DASSIMP_WARNINGS_AS_ERRORS=OFF' `
    '-DASSIMP_INSTALL=OFF' `
    '-DASSIMP_INJECT_DEBUG_POSTFIX=OFF' `
    '-DLIBRARY_SUFFIX:STRING=' `
    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_${outputConfig}:PATH=$runtimeDir" `
    "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_${outputConfig}:PATH=$libraryDir"
if ($LASTEXITCODE -ne 0) { throw 'Assimp configuration failed' }

& cmake --build $buildDir --config $cmakeConfig --target assimp --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Assimp build failed' }
