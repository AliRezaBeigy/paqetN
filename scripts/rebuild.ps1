# Rebuild script for paqetN (PowerShell)
# This script cleans and rebuilds the project

param(
    [string]$BuildDir = "build",
    [string]$QtPath = ""
)

$ErrorActionPreference = "Stop"

Write-Host "paqetN Rebuild Script" -ForegroundColor Yellow
Write-Host "====================" -ForegroundColor Yellow
Write-Host ""

# Create build directory if it doesn't exist
if (-not (Test-Path $BuildDir)) {
    Write-Host "Build directory not found, creating..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Clean build directory (but keep it for faster rebuild)
Write-Host "Cleaning build directory..." -ForegroundColor Yellow
Push-Location $BuildDir
Remove-Item -Path CMakeFiles, CMakeCache.txt, *.cmake, paqetN/, apppaqetN*, tst_paqetn*, .qt/ -Recurse -Force -ErrorAction SilentlyContinue
Pop-Location

# Check for Qt path
if ([string]::IsNullOrEmpty($env:CMAKE_PREFIX_PATH) -and [string]::IsNullOrEmpty($QtPath)) {
    Write-Host "Warning: CMAKE_PREFIX_PATH not set and -QtPath not provided" -ForegroundColor Red
    Write-Host "Please set CMAKE_PREFIX_PATH or use -QtPath parameter, e.g.:" -ForegroundColor Red
    Write-Host '  $env:CMAKE_PREFIX_PATH = "C:\Qt\6.8\mingw1310_64"' -ForegroundColor Cyan
    Write-Host "Or:" -ForegroundColor Red
    Write-Host '  .\scripts\rebuild.ps1 -QtPath "C:\Qt\6.8\mingw1310_64"' -ForegroundColor Cyan
    Write-Host ""
    $continue = Read-Host "Press Enter to continue anyway or Ctrl+C to exit"
}

# Set Qt path if provided
if (-not [string]::IsNullOrEmpty($QtPath)) {
    $env:CMAKE_PREFIX_PATH = $QtPath
    Write-Host "Using Qt path: $QtPath" -ForegroundColor Green
}

# Specify MinGW compiler (adjust path as needed)
$MinGWPath = "C:\Qt\Tools\mingw1310_64"
if (Test-Path "$MinGWPath\bin\gcc.exe") {
    Write-Host "Using MinGW from: $MinGWPath" -ForegroundColor Green
    $CompilerArgs = @(
        "-DCMAKE_C_COMPILER=$MinGWPath\bin\gcc.exe",
        "-DCMAKE_CXX_COMPILER=$MinGWPath\bin\c++.exe"
    )
} else {
    Write-Host "MinGW not found at $MinGWPath, using system compiler" -ForegroundColor Yellow
    $CompilerArgs = @()
}

# Configure
Write-Host "Configuring CMake..." -ForegroundColor Yellow
$cmakeArgs = @("-B", $BuildDir) + $CompilerArgs
& cmake @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Build
Write-Host "Building..." -ForegroundColor Yellow
cmake --build $BuildDir

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Build successful!" -ForegroundColor Green
Write-Host ""
Write-Host "Run the application with:" -ForegroundColor Cyan
Write-Host "  .\$BuildDir\apppaqetN.exe" -ForegroundColor White
Write-Host ""
Write-Host "Run tests with:" -ForegroundColor Cyan
Write-Host "  cd $BuildDir; .\tst_paqetn.exe -o test_results.txt,txt" -ForegroundColor White
