param(
    [Parameter(Mandatory = $true)]
    [string]$Source,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [ValidateSet('Debug', 'Release', 'Profile')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$dxcCommand = Get-Command dxc.exe -ErrorAction SilentlyContinue

if (!$dxcCommand) {
    throw 'DXC was not found. Add the directory containing dxc.exe to PATH and restart Visual Studio.'
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$shaderName = [System.IO.Path]::GetFileNameWithoutExtension($Source)
$shaderFlags = if ($Configuration -eq 'Debug') { @('-Zi', '-Qembed_debug', '-Od') } else { @('-O3') }

foreach ($stage in @('vs', 'ps')) {
    $entryPoint = if ($stage -eq 'vs') { 'VSMain' } else { 'PSMain' }
    $outputFile = Join-Path $OutputDirectory "$shaderName.$stage.dxil"

    & $dxcCommand.Source -T "${stage}_6_5" -E $entryPoint -I (Split-Path $Source -Parent) `
        @shaderFlags -Fo $outputFile $Source

    if ($LASTEXITCODE -ne 0) {
        throw "Shader compilation failed: $Source ($entryPoint)"
    }
}
