# Static build script for paqetN (Windows)
# Creates a fully static release build with all dependencies bundled
# Usage: .\static-build.ps1 [-QtPath "path\to\qt-static"] [-Test] [-Clean]

param(
    [string]$QtPath = "",
    [string]$BuildDir = "build-static",
    [switch]$Test,
    [switch]$Clean,
    [switch]$Release
)

$ErrorActionPreference = "Stop"

Write-Host "paqetN Static Build Script" -ForegroundColor Yellow
Write-Host "==========================" -ForegroundColor Yellow
Write-Host ""

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir

# Move to repo root
Push-Location $RepoRoot

try {
    # Clean build directory if requested
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Host "Cleaning build directory..." -ForegroundColor Yellow
        Remove-Item -Path $BuildDir -Recurse -Force
    }

    # Create build directory
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }
    
    # Always remove CMakeCache.txt to avoid stale configuration
    $CacheFile = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path $CacheFile) {
        Write-Host "Removing stale CMake cache..." -ForegroundColor Yellow
        Remove-Item -Path $CacheFile -Force
    }

    # Find Qt static installation
    $QtStaticDir = $null
    $QtSearchPaths = @(
        $QtPath,
        $env:QT_STATIC_PATH,
        "C:\qtb\qt-static",
        (Join-Path $RepoRoot "qt-static"),
        "C:\Qt\6.10.2-static\mingw_64",
        "C:\Qt\6.8.0-static\mingw_64"
    )
    
    foreach ($path in $QtSearchPaths) {
        if ($path -and (Test-Path (Join-Path $path "lib\cmake\Qt6"))) {
            $QtStaticDir = $path
            break
        }
    }

    if (-not $QtStaticDir) {
        Write-Host "Error: Static Qt installation not found!" -ForegroundColor Red
        Write-Host ""
        Write-Host "Searched locations:" -ForegroundColor Yellow
        foreach ($path in $QtSearchPaths) {
            if ($path) {
                Write-Host "  - $path" -ForegroundColor DarkGray
            }
        }
        Write-Host ""
        Write-Host "Please provide the path to a static Qt build using one of:" -ForegroundColor Yellow
        Write-Host "  1. -QtPath parameter: .\static-build.ps1 -QtPath 'C:\path\to\qt-static'" -ForegroundColor Cyan
        Write-Host "  2. QT_STATIC_PATH environment variable" -ForegroundColor Cyan
        Write-Host "  3. Place qt-static folder in repo root" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "To build static Qt, follow the Qt documentation or use the CI workflow." -ForegroundColor Yellow
        exit 1
    }

    # Verify Qt6 cmake files exist
    $Qt6CmakePath = Join-Path $QtStaticDir "lib\cmake\Qt6"
    if (-not (Test-Path $Qt6CmakePath)) {
        Write-Host "Error: Qt6 cmake files not found at $Qt6CmakePath" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Using static Qt: $QtStaticDir" -ForegroundColor Green

    # Find MinGW compiler - prefer MSYS2 as it has required static libraries
    $MinGWBin = $null
    $StaticLibsDir = $null
    
    # MSYS2 MinGW64 is preferred because it includes static compression libraries
    # (libzstd.a, libbrotlidec.a, etc.) required for static Qt builds
    $Msys2Bin = "C:\msys64\mingw64\bin"
    $Msys2Lib = "C:\msys64\mingw64\lib"
    
    if ((Test-Path (Join-Path $Msys2Bin "gcc.exe")) -and (Test-Path (Join-Path $Msys2Lib "libzstd.a"))) {
        $MinGWBin = $Msys2Bin
        $StaticLibsDir = $Msys2Lib
        Write-Host "Using MSYS2 MinGW64: $MinGWBin (has static libs)" -ForegroundColor Green
    } else {
        # Fall back to Qt MinGW, but warn about missing static libs
        $QtMinGWPaths = @(
            "C:\Qt\Tools\mingw1310_64\bin",
            "C:\Qt\Tools\mingw1120_64\bin",
            "C:\Qt\Tools\mingw_64\bin"
        )
        
        foreach ($path in $QtMinGWPaths) {
            if (Test-Path (Join-Path $path "gcc.exe")) {
                $MinGWBin = $path
                break
            }
        }
        
        if ($MinGWBin) {
            Write-Host "Using Qt MinGW: $MinGWBin" -ForegroundColor Yellow
            Write-Host ""
            Write-Host "WARNING: Qt MinGW does not include static compression libraries." -ForegroundColor Red
            Write-Host "The build may fail linking libzstd.a, libbrotlidec.a, etc." -ForegroundColor Red
            Write-Host ""
            Write-Host "To fix this, install MSYS2 and the required packages:" -ForegroundColor Yellow
            Write-Host "  1. Install MSYS2 from https://www.msys2.org/" -ForegroundColor Cyan
            Write-Host "  2. Run in MSYS2 MINGW64 terminal:" -ForegroundColor Cyan
            Write-Host "     pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-zstd mingw-w64-x86_64-brotli mingw-w64-x86_64-zlib" -ForegroundColor White
            Write-Host ""
        }
    }

    $GccPath = $null
    $GppPath = $null
    
    if (-not $MinGWBin) {
        Write-Host "Warning: MinGW not found in standard locations, using system compiler" -ForegroundColor Yellow
    } else {
        # Add MinGW to PATH
        $env:PATH = "$MinGWBin;$env:PATH"
        
        $GccPath = Join-Path $MinGWBin "gcc.exe"
        $GppPath = Join-Path $MinGWBin "g++.exe"
    }

    # Build type
    $BuildType = if ($Release) { "Release" } else { "RelWithDebInfo" }

    # Configure CMake for static build
    Write-Host ""
    Write-Host "Configuring CMake for static build ($BuildType)..." -ForegroundColor Yellow
    
    # Build cmake command - avoid array splatting issues by calling cmake directly
    $cmakeCmd = "cmake -B `"$BuildDir`" -G Ninja -DCMAKE_BUILD_TYPE=$BuildType"
    $cmakeCmd += " -DCMAKE_PREFIX_PATH=`"$QtStaticDir`""
    $cmakeCmd += " -DBUILD_SHARED_LIBS=OFF"
    $cmakeCmd += " ""-DCMAKE_EXE_LINKER_FLAGS=-static -static-libgcc -static-libstdc++"""
    
    if ($GccPath) {
        $cmakeCmd += " -DCMAKE_C_COMPILER=`"$GccPath`""
        $cmakeCmd += " -DCMAKE_CXX_COMPILER=`"$GppPath`""
    }
    if ($StaticLibsDir) {
        $cmakeCmd += " -DSTATIC_LIBS_DIR=`"$StaticLibsDir`""
    }

    Write-Host $cmakeCmd -ForegroundColor DarkGray
    Invoke-Expression $cmakeCmd

    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configuration failed!" -ForegroundColor Red
        exit 1
    }

    # Build
    Write-Host ""
    Write-Host "Building..." -ForegroundColor Yellow
    
    $Jobs = $env:NUMBER_OF_PROCESSORS
    if (-not $Jobs) { $Jobs = 4 }
    
    cmake --build $BuildDir --parallel $Jobs

    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }

    Write-Host ""
    Write-Host "Build successful!" -ForegroundColor Green

    # Find the executable
    $ExePath = Join-Path $BuildDir "apppaqetN.exe"
    if (Test-Path $ExePath) {
        $FileInfo = Get-Item $ExePath
        $SizeMB = [math]::Round($FileInfo.Length / 1MB, 2)
        Write-Host "Executable: $ExePath ($SizeMB MB)" -ForegroundColor Cyan
    }

    # Test if requested
    if ($Test) {
        Write-Host ""
        Write-Host "Testing static build..." -ForegroundColor Yellow
        
        if (Test-Path $ExePath) {
            Write-Host "Launching application..." -ForegroundColor Cyan
            Start-Process -FilePath $ExePath
            Write-Host "Application launched. Check if it runs without errors." -ForegroundColor Green
        } else {
            Write-Host "Warning: Executable not found at $ExePath" -ForegroundColor Red
        }
    }

    Write-Host ""
    Write-Host "Static build complete!" -ForegroundColor Green
    Write-Host ""
    Write-Host "To run the application:" -ForegroundColor Cyan
    Write-Host "  .\$BuildDir\apppaqetN.exe" -ForegroundColor White
    Write-Host ""
    Write-Host "To test the build:" -ForegroundColor Cyan
    Write-Host "  .\scripts\static-build.ps1 -Test" -ForegroundColor White

} finally {
    Pop-Location
}
