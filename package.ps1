# Builds PON (Release) and creates a drop-in ZIP for distribution:
#   dist\PON-<version>-win64.zip  =  PON.vst3\ + README.txt
# Usage:  powershell -ExecutionPolicy Bypass -File .\package.ps1  [-SkipBuild]
param([switch]$SkipBuild)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if (-not $SkipBuild) {
    & powershell -ExecutionPolicy Bypass -File .\build.ps1
    if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: build failed." -ForegroundColor Red; exit 1 }
}

$version = (Select-String -Path CMakeLists.txt -Pattern 'project\(PON VERSION ([0-9.]+)').Matches[0].Groups[1].Value
$vst3 = Join-Path $PSScriptRoot "build\PON_artefacts\Release\VST3\PON.vst3"
if (-not (Test-Path $vst3)) { Write-Host "ERROR: $vst3 not found." -ForegroundColor Red; exit 1 }

$stage = Join-Path $PSScriptRoot "dist\stage"
$zip   = Join-Path $PSScriptRoot "dist\PON-$version-win64.zip"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null

Copy-Item -Recurse $vst3 $stage
Copy-Item (Join-Path $PSScriptRoot "dist\README.txt") $stage

if (Test-Path $zip) { Remove-Item -Force $zip }
# Write entries with standard '/' separators (Compress-Archive / ZipFile on Windows PowerShell 5.1
# write '\', which non-Windows unzip tools mis-handle).
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
try {
    Get-ChildItem -Recurse -File $stage | ForEach-Object {
        $entryName = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $_.FullName, $entryName, [IO.Compression.CompressionLevel]::Optimal)
    }
} finally {
    $archive.Dispose()
}
Remove-Item -Recurse -Force $stage

Write-Host ""
Write-Host "PACKAGE CREATED: $zip" -ForegroundColor Green
