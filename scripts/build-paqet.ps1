# Build paqet binary for desktop (Windows).
# Usage: .\build-paqet.ps1 [-PaqetSrc "path\to\paqet"]
# Requires: Go installed. Paqet source at $PaqetSrc (default: ..\paqet relative to script, or env PAQET_SRC).
# Output: paqet.exe in build directory; copy next to apppaqetN.exe for use.

param(
    [string]$PaqetSrc = $env:PAQET_SRC
)
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir
if (-not $PaqetSrc) { $PaqetSrc = Join-Path $RepoRoot "paqet" }
if (-not (Test-Path (Join-Path $PaqetSrc "go.mod"))) {
    Write-Error "paqet source not found at $PaqetSrc. Set PAQET_SRC or pass -PaqetSrc."
}
$BuildDir = Join-Path $RepoRoot "build"
$OutDir = Join-Path $BuildDir "paqet"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$env:GOOS = "windows"
$env:GOARCH = "amd64"
$Output = Join-Path $OutDir "paqet.exe"
Write-Host "Building paqet for windows/amd64 into $Output" -ForegroundColor Cyan
Push-Location $PaqetSrc
go build -o $Output ./cmd/...
$exitCode = $LASTEXITCODE
Pop-Location
if ($exitCode -eq 0) { Write-Host "Done. Copy $Output next to apppaqetN.exe." -ForegroundColor Green }
exit $exitCode
