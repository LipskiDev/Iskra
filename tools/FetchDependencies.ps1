$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& git -C $projectRoot submodule update --init --recursive -- external/glm
if ($LASTEXITCODE -ne 0) { throw 'GLM submodule initialization failed' }
$dependencies = @(
    @{ Name = 'glfw'; Url = 'https://github.com/glfw/glfw.git'; Revision = '7b6aead9fb88b3623e3b3725ebb42670cbe4c579' },
    @{ Name = 'nvrhi'; Url = 'https://github.com/NVIDIA-RTX/NVRHI.git'; Revision = 'bf0bbd9b666a7297492beb1999a4ddc0dfded143' },
    @{ Name = 'DirectX-Headers'; Url = 'https://github.com/microsoft/DirectX-Headers.git'; Revision = 'd873b344dc540898868697245f100c2a67fe68d9' }
)
foreach ($dependency in $dependencies) {
    $destination = Join-Path $projectRoot ('external/' + $dependency.Name)
    if (Test-Path -LiteralPath $destination) {
        $revision = & git -C $destination rev-parse HEAD
        if ($LASTEXITCODE -ne 0 -or $revision -ne $dependency.Revision) {
            throw "Existing $destination does not match the pinned revision; resolve manually."
        }
        continue
    }
    & git init $destination
    if ($LASTEXITCODE -ne 0) { throw 'git init failed' }
    & git -C $destination remote add origin $dependency.Url
    if ($LASTEXITCODE -ne 0) { throw 'git remote failed' }
    & git -C $destination fetch --depth 1 origin $dependency.Revision
    if ($LASTEXITCODE -ne 0) { throw 'git fetch failed' }
    & git -C $destination checkout --detach FETCH_HEAD
    if ($LASTEXITCODE -ne 0) { throw 'git checkout failed' }
}
