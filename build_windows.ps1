#
# FrankyCPP
# Copyright (c) 2018-2026 Frank Kopp
#
# MIT License
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

param(
    [Parameter(Position=0)]
    [ValidateSet("debug", "release", "relwithdebinfo", "minsizerel")]
    [string]$BuildMode = "release",

    [switch]$Help
)

# Show help
if ($Help) {
    Write-Host @"
FrankyCPP - Windows Build Script

Usage: .\build_windows.ps1 [BuildMode] [-Help]

Parameters:
  BuildMode         Build configuration (default: release)
                    Valid values: debug, release, relwithdebinfo, minsizerel
  -Help            Show this help message

Examples:
  .\build_windows.ps1                    # Release build (default)
  .\build_windows.ps1 debug              # Debug build
  .\build_windows.ps1 relwithdebinfo     # Release with debug info
  .\build_windows.ps1 -Help              # Show this help

Notes:
  - Works in both normal PowerShell and Developer PowerShell for VS
  - Automatically initializes MSVC environment if needed
  - VCPKG_ROOT environment variable must be set
  - Requires Visual Studio 2019 16.10+ or Visual Studio 2022
  - Requires CMake 3.16+ and Ninja
  - First build may take 5-10 minutes (vcpkg dependencies with parallel builds)
  - Subsequent builds are much faster (1-2 minutes)

GitHub Actions:
  - GitHub runners have MSVC pre-configured
  - This script will work without modification in CI/CD

"@
    exit 0
}

$ErrorActionPreference = "Stop"

Write-Host "=========================================="
Write-Host "FrankyCPP - Windows Build Script"
Write-Host "Build Mode: $BuildMode"
Write-Host "=========================================="

# Validate VCPKG_ROOT
if (-not $env:VCPKG_ROOT) {
    Write-Host "ERROR: VCPKG_ROOT environment variable is not set" -ForegroundColor Red
    Write-Host ""
    Write-Host "Checking for CLion vcpkg installation..."
    $clionVcpkg = "$env:USERPROFILE\.vcpkg-clion\vcpkg"
    if (Test-Path $clionVcpkg) {
        Write-Host "Found CLion vcpkg at: $clionVcpkg" -ForegroundColor Yellow
        Write-Host "Set it with: `$env:VCPKG_ROOT = '$clionVcpkg'" -ForegroundColor Yellow
        Write-Host "Or run: .\setup_windows_build_env.ps1 to configure automatically" -ForegroundColor Yellow
    } else {
        Write-Host "No vcpkg found. Options:" -ForegroundColor Yellow
        Write-Host "  1. Run: .\setup_windows_build_env.ps1 -Install" -ForegroundColor Yellow
        Write-Host "  2. Or manually set VCPKG_ROOT to your vcpkg installation" -ForegroundColor Yellow
    }
    exit 1
}

Write-Host "Using vcpkg from: $env:VCPKG_ROOT"

# Validate vcpkg exists
if (-not (Test-Path "$env:VCPKG_ROOT\.vcpkg-root")) {
    Write-Host "ERROR: Invalid VCPKG_ROOT - .vcpkg-root file not found" -ForegroundColor Red
    Write-Host "Path: $env:VCPKG_ROOT" -ForegroundColor Red
    exit 1
}

# Check for Visual Studio / MSVC environment
Write-Host "Checking MSVC environment..."
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

# Check if we're already in a Developer PowerShell (MSVC environment initialized)
$inDevShell = $false
if ($env:VSCMD_ARG_TGT_ARCH -or $env:VCINSTALLDIR) {
    $inDevShell = $true
    Write-Host "[OK]" -ForegroundColor Green -NoNewline
    Write-Host " Running in Developer PowerShell (MSVC environment active)"
} else {
    Write-Host "[INFO]" -ForegroundColor Yellow -NoNewline
    Write-Host " Not in Developer PowerShell - will initialize MSVC environment"
}

# Find Visual Studio installation
if (-not (Test-Path $vsWhere)) {
    Write-Host "ERROR: vswhere.exe not found - Visual Studio may not be installed" -ForegroundColor Red
    Write-Host "Install Visual Studio 2019 16.10+ or 2022 with C++ Desktop Development workload" -ForegroundColor Red
    exit 1
}

$vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
if (-not $vsPath) {
    Write-Host "ERROR: Visual Studio with C++ tools not found" -ForegroundColor Red
    Write-Host "Install Visual Studio 2019 16.10+ or 2022 with C++ Desktop Development workload" -ForegroundColor Red
    exit 1
}

Write-Host "Found Visual Studio at: $vsPath"

# Initialize MSVC environment if not already in Developer PowerShell
if (-not $inDevShell) {
    Write-Host ""
    Write-Host "Initializing MSVC environment for x64..."

    # Find vcvarsall.bat
    $vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsall)) {
        Write-Host "ERROR: vcvarsall.bat not found at: $vcvarsall" -ForegroundColor Red
        exit 1
    }

    # Create a temporary batch file that calls vcvarsall and outputs environment
    $tempBat = [System.IO.Path]::GetTempFileName() + ".bat"
    $tempOut = [System.IO.Path]::GetTempFileName()

    # Write batch file that calls vcvarsall and dumps environment to file
    @"
@echo off
call "$vcvarsall" x64 > nul 2>&1
if errorlevel 1 (
    echo ERROR: vcvarsall.bat failed
    exit /b 1
)
set > "$tempOut"
"@ | Out-File -FilePath $tempBat -Encoding ASCII

    # Run the batch file
    $result = cmd.exe /c "$tempBat"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Failed to initialize MSVC environment" -ForegroundColor Red
        Remove-Item $tempBat -ErrorAction SilentlyContinue
        Remove-Item $tempOut -ErrorAction SilentlyContinue
        exit 1
    }

    # Read environment variables from output file
    if (-not (Test-Path $tempOut)) {
        Write-Host "ERROR: Failed to capture environment variables" -ForegroundColor Red
        Remove-Item $tempBat -ErrorAction SilentlyContinue
        exit 1
    }

    $envOutput = Get-Content $tempOut

    # Parse and apply ALL environment variables from vcvarsall
    $envVarsSet = 0
    foreach ($line in $envOutput) {
        if ($line -match '^([^=]+)=(.*)$') {
            $name = $matches[1]
            $value = $matches[2]

            # Set the environment variable
            # Use [Environment]::SetEnvironmentVariable for proper handling
            try {
                [System.Environment]::SetEnvironmentVariable($name, $value, [System.EnvironmentVariableTarget]::Process)
                $envVarsSet++
            } catch {
                # Silently skip any variables that can't be set
            }
        }
    }

    # Clean up temp files
    Remove-Item $tempBat -ErrorAction SilentlyContinue
    Remove-Item $tempOut -ErrorAction SilentlyContinue

    Write-Host "[OK]" -ForegroundColor Green -NoNewline
    Write-Host " MSVC environment initialized ($envVarsSet variables set)"
    Write-Host ""

    # Verify that cl.exe is now in PATH
    $clExe = Get-Command cl.exe -ErrorAction SilentlyContinue
    if ($clExe) {
        Write-Host "[OK]" -ForegroundColor Green -NoNewline
        Write-Host " MSVC compiler available: $($clExe.Path)"
    } else {
        Write-Host "ERROR: cl.exe not found in PATH after environment initialization" -ForegroundColor Red
        Write-Host "This should not happen. Please run from 'Developer PowerShell for VS 2022'" -ForegroundColor Red
        Write-Host ""
        Write-Host "To open Developer PowerShell:" -ForegroundColor Yellow
        Write-Host "  1. Search for 'Developer PowerShell for VS 2022' in Start Menu" -ForegroundColor Yellow
        Write-Host "  2. Or run: & '$vsPath\Common7\Tools\Launch-VsDevShell.ps1'" -ForegroundColor Yellow
        exit 1
    }

    Write-Host ""
}

# Enable parallel builds for vcpkg (speeds up Boost compilation significantly)
$cpuCores = (Get-CimInstance -ClassName Win32_Processor | Measure-Object -Property NumberOfLogicalProcessors -Sum).Sum
$env:VCPKG_MAX_CONCURRENCY = $cpuCores
Write-Host "Enabling vcpkg parallel builds: $cpuCores cores"
Write-Host ""

# Configure using CMake preset
$preset = $BuildMode
Write-Host ""
Write-Host "Configuring with preset: $preset"
Write-Host ""

try {
    cmake --preset $preset
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: CMake configuration failed" -ForegroundColor Red
        exit $LASTEXITCODE
    }
} catch {
    Write-Host "ERROR: CMake configuration failed: $_" -ForegroundColor Red
    exit 1
}

# Build
$buildDir = "cmake-build-$preset"
Write-Host ""
Write-Host "Building in: $buildDir"
Write-Host ""

try {
    cmake --build $buildDir --config $BuildMode --parallel
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Build failed" -ForegroundColor Red
        exit $LASTEXITCODE
    }
} catch {
    Write-Host "ERROR: Build failed: $_" -ForegroundColor Red
    exit 1
}

# Run tests (excluding slow tests)
Write-Host ""
Write-Host "Running tests..."
# Find test executable (version-independent)
$testExe = Get-ChildItem -Path ".\$buildDir\test" -Filter "FrankyCPP_v*_Test.exe" -File -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
if ($testExe -and (Test-Path $testExe)) {
    try {
        & $testExe --gtest_filter=-*SpeedTests.*:*TimingTests.*
        if ($LASTEXITCODE -ne 0) {
            Write-Host "WARNING: Some tests failed" -ForegroundColor Yellow
        }
    } catch {
        Write-Host "WARNING: Test execution failed: $_" -ForegroundColor Yellow
    }
} else {
    Write-Host "WARNING: Test executable not found in .\$buildDir\test\" -ForegroundColor Yellow
}

# Show engine executable location (version-independent)
$exePath = Get-ChildItem -Path ".\$buildDir\src" -Filter "FrankyCPP_v*.exe" -File -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
# Show engine executable location (version-independent)
$exePath = Get-ChildItem -Path ".\$buildDir\src" -Filter "FrankyCPP_v*.exe" -File -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
Write-Host ""
if ($exePath -and (Test-Path $exePath)) {
    Write-Host "=========================================="
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host "Engine: $exePath"
    Write-Host "=========================================="
} else {
    Write-Host "WARNING: Engine executable not found in .\$buildDir\src\" -ForegroundColor Yellow
    Write-Host "Build may have failed or executable is in a different location" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Build completed at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
