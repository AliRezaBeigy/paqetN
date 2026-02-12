# Clean persistent data script for paqetN (Windows)
# Removes all user data to test the app from a fresh state
# Usage: .\clean-persistents.ps1 [-Confirm] [-WhatIf]

param(
    [switch]$Confirm,
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"

Write-Host "paqetN Clean Persistents Script" -ForegroundColor Yellow
Write-Host "================================" -ForegroundColor Yellow
Write-Host ""

# Define all persistent data locations
$AppDataDir = Join-Path $env:APPDATA "paqetN"
$LocalAppDataDir = Join-Path $env:LOCALAPPDATA "paqetN"
$RegistryPath = "HKCU:\Software\paqetN"

# Collect items to clean
$ItemsToClean = @()

# AppData folder (configs, cache, etc.)
if (Test-Path $AppDataDir) {
    $ItemsToClean += @{
        Type = "Directory"
        Path = $AppDataDir
        Description = "Application data (configs.json, settings)"
    }
}

# LocalAppData folder (if exists)
if (Test-Path $LocalAppDataDir) {
    $ItemsToClean += @{
        Type = "Directory"
        Path = $LocalAppDataDir
        Description = "Local application data (cache)"
    }
}

# Registry settings
if (Test-Path $RegistryPath) {
    $ItemsToClean += @{
        Type = "Registry"
        Path = $RegistryPath
        Description = "Registry settings (theme, ports, preferences)"
    }
}

# Check if there's anything to clean
if ($ItemsToClean.Count -eq 0) {
    Write-Host "No persistent data found. The app is already in a fresh state." -ForegroundColor Green
    exit 0
}

# Show what will be cleaned
Write-Host "The following persistent data will be removed:" -ForegroundColor Yellow
Write-Host ""

foreach ($item in $ItemsToClean) {
    Write-Host "  [$($item.Type)]" -ForegroundColor Cyan -NoNewline
    Write-Host " $($item.Path)" -ForegroundColor White
    Write-Host "    $($item.Description)" -ForegroundColor DarkGray
    
    # Show contents for directories
    if ($item.Type -eq "Directory" -and (Test-Path $item.Path)) {
        $files = Get-ChildItem -Path $item.Path -Recurse -File -ErrorAction SilentlyContinue
        if ($files) {
            Write-Host "    Files: $($files.Count)" -ForegroundColor DarkGray
            foreach ($file in $files | Select-Object -First 5) {
                $relativePath = $file.FullName.Substring($item.Path.Length + 1)
                Write-Host "      - $relativePath" -ForegroundColor DarkGray
            }
            if ($files.Count -gt 5) {
                Write-Host "      ... and $($files.Count - 5) more" -ForegroundColor DarkGray
            }
        }
    }
    
    # Show registry keys
    if ($item.Type -eq "Registry" -and (Test-Path $item.Path)) {
        try {
            $regValues = Get-ItemProperty -Path $item.Path -ErrorAction SilentlyContinue
            if ($regValues) {
                $valueNames = $regValues.PSObject.Properties | 
                    Where-Object { $_.Name -notmatch '^PS' } | 
                    Select-Object -ExpandProperty Name -First 5
                foreach ($name in $valueNames) {
                    Write-Host "      - $name" -ForegroundColor DarkGray
                }
            }
        } catch {}
    }
    
    Write-Host ""
}

# WhatIf mode - just show what would be done
if ($WhatIf) {
    Write-Host "WhatIf: No changes were made." -ForegroundColor Yellow
    exit 0
}

# Confirm before deleting
if (-not $Confirm) {
    Write-Host "This will permanently delete all paqetN user data!" -ForegroundColor Red
    $response = Read-Host "Are you sure? (y/N)"
    if ($response -ne 'y' -and $response -ne 'Y') {
        Write-Host "Cancelled." -ForegroundColor Yellow
        exit 0
    }
}

# Make sure paqetN is not running
$paqetProcesses = Get-Process -Name "apppaqetN", "paqetN" -ErrorAction SilentlyContinue
if ($paqetProcesses) {
    Write-Host ""
    Write-Host "Warning: paqetN is currently running!" -ForegroundColor Red
    $response = Read-Host "Close the application and continue? (y/N)"
    if ($response -eq 'y' -or $response -eq 'Y') {
        Write-Host "Stopping paqetN processes..." -ForegroundColor Yellow
        $paqetProcesses | Stop-Process -Force
        Start-Sleep -Seconds 1
    } else {
        Write-Host "Cancelled. Please close paqetN manually and try again." -ForegroundColor Yellow
        exit 0
    }
}

# Perform cleanup
Write-Host ""
Write-Host "Cleaning..." -ForegroundColor Yellow

$cleaned = 0
$failed = 0

foreach ($item in $ItemsToClean) {
    Write-Host "  Removing $($item.Path)... " -NoNewline
    
    try {
        if ($item.Type -eq "Directory") {
            Remove-Item -Path $item.Path -Recurse -Force -ErrorAction Stop
        } elseif ($item.Type -eq "Registry") {
            Remove-Item -Path $item.Path -Recurse -Force -ErrorAction Stop
        }
        Write-Host "OK" -ForegroundColor Green
        $cleaned++
    } catch {
        Write-Host "FAILED" -ForegroundColor Red
        Write-Host "    Error: $($_.Exception.Message)" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
if ($failed -eq 0) {
    Write-Host "All persistent data cleaned successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "The app will start fresh on next launch." -ForegroundColor Cyan
} else {
    Write-Host "Cleaned $cleaned items, $failed failed." -ForegroundColor Yellow
}
