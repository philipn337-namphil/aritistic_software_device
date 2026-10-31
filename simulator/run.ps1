# Starts the virtual device and opens it in the browser.
#   .\simulator\run.ps1                 real dev server (http://52.79.82.17)
#   .\simulator\run.ps1 -Mock           also start the local mock API on :9090
#   .\simulator\run.ps1 -Lan            listen on all interfaces (phones on the same Wi-Fi)
#   .\simulator\run.ps1 -Data demo-b    separate device (own UID, pairing, records in simulator/data/demo-b)
param(
    [int]$Port = 8080,
    [string]$Data = 'default',
    [switch]$Mock,
    [switch]$Lan,
    [switch]$Rebuild,
    [switch]$NoBrowser
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'simulator/build/yeso-sim.exe'
$bash = 'C:/Program Files/Git/bin/bash.exe'
if (-not (Test-Path $bash)) { $bash = 'bash' }

$pak = Join-Path $root 'simulator/assets/ui.pak'
if (-not (Test-Path $pak)) {
    throw "Missing $pak. Copy assets/packed/ui.pak from the Yeso2026 project (7,182,088 bytes)."
}

$stale = $Rebuild -or -not (Test-Path $exe)
if (-not $stale) {
    $built = (Get-Item $exe).LastWriteTime
    $stale = [bool](Get-ChildItem (Join-Path $root 'simulator/src'), (Join-Path $root 'firmware/RoutineDevice/src') -Recurse -File |
        Where-Object { $_.Extension -in '.h', '.cpp' -and $_.LastWriteTime -gt $built } | Select-Object -First 1)
}
if ($stale) {
    Write-Host 'Building virtual device...'
    & $bash (Join-Path $root 'simulator/scripts/build.sh')
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
}

if ($Mock) {
    Start-Process -WindowStyle Minimized python -ArgumentList (Join-Path $root 'simulator/scripts/mock_server.py'), '9090'
    Write-Host 'Mock API: http://127.0.0.1:9090  (console: api reset-uuid / api endpoint http://127.0.0.1:9090 / api pair 1234567890)'
}

New-Item -ItemType Directory -Force -Path (Join-Path $root 'simulator/data') | Out-Null
$listen = if ($Lan) { '0.0.0.0' } else { '127.0.0.1' }
$url = "http://localhost:$Port/"
if (-not $NoBrowser) { Start-Job { Start-Sleep 1; Start-Process $using:url } | Out-Null }
Push-Location $root
try {
    & $exe --port $Port --host $listen --data "simulator/data/$Data" --pak 'simulator/assets/ui.pak' --web 'simulator/web'
} finally { Pop-Location }
