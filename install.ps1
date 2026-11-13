# Copies PON.vst3 to C:\Program Files\Common Files\VST3 (asks for admin rights)
# Usage:  powershell -ExecutionPolicy Bypass -File .\install.ps1
# Close the DAW first: while FL Studio etc. has PON loaded, the DLL is locked and cannot be replaced.
$src = Join-Path $PSScriptRoot "build\PON_artefacts\Release\VST3\PON.vst3"
$dst = "C:\Program Files\Common Files\VST3"
$bin = "Contents\x86_64-win\PON.vst3"

if (-not (Test-Path $src)) {
    Write-Host "ERROR: $src not found. Run build.ps1 first." -ForegroundColor Red
    exit 1
}

$daws = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match '^(FL64|FL|ilbridge|Ableton|Cubase|reaper|Studio One)' }
if ($daws) {
    Write-Host ("WARNING: DAW is running (" + (($daws | Select-Object -ExpandProperty ProcessName -Unique) -join ", ") + "). If it has PON loaded, the copy will fail.") -ForegroundColor Yellow
}

$cmd = "New-Item -ItemType Directory -Force '$dst' | Out-Null; " +
       "if (Test-Path '$dst\PON.vst3') { Remove-Item -Recurse -Force '$dst\PON.vst3' }; " +
       "Copy-Item -Recurse -Force '$src' '$dst'"

Start-Process powershell -Verb RunAs -Wait -ArgumentList "-NoProfile", "-Command", $cmd

# Verify by hash that the installed binary really is the new one
$new = Join-Path $src $bin
$old = Join-Path "$dst\PON.vst3" $bin
if ((Test-Path $old) -and ((Get-FileHash $new).Hash -eq (Get-FileHash $old).Hash)) {
    Write-Host "INSTALLED: $dst\PON.vst3" -ForegroundColor Green
} else {
    Write-Host "ERROR: copy failed - the installed PON is still the old one. Close the DAW (FL Studio) and run install.ps1 again." -ForegroundColor Red
    exit 1
}
