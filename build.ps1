# PON build script
# Usage:  powershell -ExecutionPolicy Bypass -File .\build.ps1          (plugin only)
#         powershell -ExecutionPolicy Bypass -File .\build.ps1 -Tests   (plugin + PONRenderTest)
param([switch]$Tests)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

function Find-CMake {
    $c = Get-Command cmake -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vs) {
            $p = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $p) { return $p }
        }
    }

    $p = "C:\Program Files\CMake\bin\cmake.exe"
    if (Test-Path $p) { return $p }
    return $null
}

$cmake = Find-CMake
if (-not $cmake) {
    Write-Host "ERROR: CMake not found. Install 'Desktop development with C++' in Visual Studio Installer." -ForegroundColor Red
    exit 1
}
Write-Host "Using CMake: $cmake" -ForegroundColor Cyan

$testFlag = if ($Tests) { "ON" } else { "OFF" }
& $cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DPON_BUILD_TESTS=$testFlag"
if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: CMake configure failed." -ForegroundColor Red; exit 1 }

& $cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: Build failed." -ForegroundColor Red; exit 1 }

$out = Join-Path $PSScriptRoot "build\PON_artefacts\Release\VST3\PON.vst3"
Write-Host ""
Write-Host "BUILD SUCCEEDED" -ForegroundColor Green
Write-Host "VST3: $out" -ForegroundColor Green
