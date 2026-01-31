#
# FrankyCPP Windows Build Environment Setup & Validation Script
# Copyright (c) 2018-2026 Frank Kopp
# MIT License
#
# This script validates and optionally sets up the build environment for Windows
#
# Usage:
#   .\setup_windows_build_env.ps1              # Validate environment (safe, no modifications)
#   .\setup_windows_build_env.ps1 -Install     # Install vcpkg and validate
#   .\setup_windows_build_env.ps1 -Help        # Show help
#

param(
    [switch]$Install,
    [switch]$SetEnvVar,
    [switch]$Help
)

# Colors
$Red = "Red"
$Green = "Green"
$Yellow = "Yellow"
$Blue = "Cyan"

if ($Help) {
    Write-Host "FrankyCPP Windows Build Environment Setup" -ForegroundColor $Blue
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  .\setup_windows_build_env.ps1              Validate environment (safe, no modifications)"
    Write-Host "  .\setup_windows_build_env.ps1 -Install     Install vcpkg and validate"
    Write-Host "  .\setup_windows_build_env.ps1 -SetEnvVar   Set VCPKG_ROOT for detected vcpkg"
    Write-Host "  .\setup_windows_build_env.ps1 -Help        Show this help message"
    Write-Host ""
    Write-Host "SAFETY NOTE:"
    Write-Host "  Default behavior is validate-only to avoid unintended system modifications."
    Write-Host "  Use -Install explicitly when you want to install vcpkg."
    Write-Host "  Use -SetEnvVar to set VCPKG_ROOT for an existing vcpkg installation."
    Write-Host ""
    Write-Host "PREREQUISITES (must be installed manually):"
    Write-Host "  - Visual Studio 2019 16.10+ or Visual Studio 2022 (with C++ Desktop Development)"
    Write-Host "  - CMake 3.16 or higher"
    Write-Host "  - Ninja build system (optional but recommended)"
    Write-Host "  - Git (usually already installed)"
    Write-Host ""
    exit 0
}

Write-Host "========================================" -ForegroundColor $Blue
Write-Host "FrankyCPP Windows Build Environment Setup" -ForegroundColor $Blue
Write-Host "========================================" -ForegroundColor $Blue
Write-Host ""

if ($Install) {
    Write-Host "Mode: INSTALLING vcpkg" -ForegroundColor $Yellow
} elseif ($SetEnvVar) {
    Write-Host "Mode: SETTING VCPKG_ROOT environment variable" -ForegroundColor $Yellow
} else {
    Write-Host "Mode: VALIDATING environment (safe, no modifications)" -ForegroundColor $Green
}
Write-Host ""

$Errors = 0
$Warnings = 0

# =============================================================================
# VALIDATION
# =============================================================================

Write-Host "========================================" -ForegroundColor $Blue
Write-Host "Validating Build Environment" -ForegroundColor $Blue
Write-Host "========================================" -ForegroundColor $Blue
Write-Host ""

# Check Visual Studio / MSVC
Write-Host "Checking Visual Studio / MSVC..." -ForegroundColor $Blue

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    Write-Host "[OK]" -ForegroundColor $Green -NoNewline
    Write-Host " vswhere found"

    # Find latest VS installation with C++ tools
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($vsPath) {
        Write-Host "[OK]" -ForegroundColor $Green -NoNewline
        Write-Host " Visual Studio found: $vsPath"

        # Check MSVC version
        $vsVersion = & $vswhere -latest -property installationVersion 2>$null
        Write-Host "  Version: $vsVersion"

        # Check if MSVC 2019 16.10+ or 2022
        $majorVersion = [int]($vsVersion.Split('.')[0])
        if ($majorVersion -ge 17) {
            Write-Host "[OK]" -ForegroundColor $Green -NoNewline
            Write-Host " Visual Studio 2022 (C++20 supported)"
        } elseif ($majorVersion -eq 16) {
            $minorVersion = [int]($vsVersion.Split('.')[1])
            if ($minorVersion -ge 10) {
                Write-Host "[OK]" -ForegroundColor $Green -NoNewline
                Write-Host " Visual Studio 2019 16.10+ (C++20 supported)"
            } else {
                Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
                Write-Host " Visual Studio 2019 $vsVersion is too old (need 16.10+ for C++20)"
                Write-Host "  Download: https://visualstudio.microsoft.com/" -ForegroundColor $Yellow
                $Errors++
            }
        } else {
            Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
            Write-Host " Visual Studio is too old (need 2019 16.10+ or 2022 for C++20)"
            Write-Host "  Download: https://visualstudio.microsoft.com/" -ForegroundColor $Yellow
            $Errors++
        }

        # Check if environment is initialized
        if ($env:VSINSTALLDIR -or $env:DevEnvDir) {
            Write-Host "[OK]" -ForegroundColor $Green -NoNewline
            Write-Host " MSVC environment initialized"
        } else {
            Write-Host "[WARN]" -ForegroundColor $Yellow -NoNewline
            Write-Host " MSVC environment not initialized"
            Write-Host "  Note: This is OK if using CLion or Visual Studio IDE" -ForegroundColor $Yellow
            Write-Host "  For command-line builds, use 'x64 Native Tools Command Prompt'" -ForegroundColor $Yellow
            $Warnings++
        }
    } else {
        Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
        Write-Host " Visual Studio C++ tools not found"
        Write-Host "  Install Visual Studio 2019 16.10+ or 2022 with 'Desktop development with C++'" -ForegroundColor $Yellow
        Write-Host "  Download: https://visualstudio.microsoft.com/" -ForegroundColor $Yellow
        $Errors++
    }
} else {
    Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
    Write-Host " Visual Studio not found (vswhere.exe missing)"
    Write-Host "  Install Visual Studio 2019 16.10+ or 2022 with 'Desktop development with C++'" -ForegroundColor $Yellow
    Write-Host "  Download: https://visualstudio.microsoft.com/" -ForegroundColor $Yellow
    $Errors++
}

Write-Host ""

# Check build tools
Write-Host "Checking build tools..." -ForegroundColor $Blue

# CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) {
    Write-Host "[OK]" -ForegroundColor $Green -NoNewline
    Write-Host " cmake found: $($cmake.Version)"

    $cmakeVersion = & cmake --version 2>&1 | Select-String -Pattern "cmake version (\d+\.\d+\.\d+)" | ForEach-Object { $_.Matches.Groups[1].Value }
    if ($cmakeVersion) {
        $versionParts = $cmakeVersion.Split('.')
        $major = [int]$versionParts[0]
        $minor = [int]$versionParts[1]
        if ($major -gt 3 -or ($major -eq 3 -and $minor -ge 16)) {
            Write-Host "[OK]" -ForegroundColor $Green -NoNewline
            Write-Host " CMake $cmakeVersion meets requirement (>= 3.16)"
        } else {
            Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
            Write-Host " CMake $cmakeVersion is too old (need >= 3.16)"
            Write-Host "  Download: https://cmake.org/download/" -ForegroundColor $Yellow
            $Errors++
        }
    }
} else {
    Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
    Write-Host " cmake not found"
    Write-Host "  Download: https://cmake.org/download/" -ForegroundColor $Yellow
    $Errors++
}

# Ninja
$ninja = Get-Command ninja -ErrorAction SilentlyContinue
if ($ninja) {
    Write-Host "[OK]" -ForegroundColor $Green -NoNewline
    $ninjaVersion = & ninja --version 2>&1
    Write-Host " ninja found: $ninjaVersion"
} else {
    Write-Host "[WARN]" -ForegroundColor $Yellow -NoNewline
    Write-Host " ninja not found (optional but recommended)"
    Write-Host "  Download: https://github.com/ninja-build/ninja/releases" -ForegroundColor $Yellow
    Write-Host "  Or install via: choco install ninja (if using Chocolatey)" -ForegroundColor $Yellow
    $Warnings++
}

# Git
$git = Get-Command git -ErrorAction SilentlyContinue
if ($git) {
    Write-Host "[OK]" -ForegroundColor $Green -NoNewline
    $gitVersion = & git --version 2>&1
    Write-Host " git found: $gitVersion"
} else {
    Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
    Write-Host " git not found"
    Write-Host "  Download: https://git-scm.com/downloads" -ForegroundColor $Yellow
    $Errors++
}

Write-Host ""

# Check vcpkg
Write-Host "Checking vcpkg..." -ForegroundColor $Blue

$vcpkgFound = $false
$vcpkgLocation = $null

if ($env:VCPKG_ROOT) {
    $vcpkgLocation = $env:VCPKG_ROOT
    Write-Host "[OK]" -ForegroundColor $Green -NoNewline
    Write-Host " VCPKG_ROOT is set: $vcpkgLocation"

    if (Test-Path $vcpkgLocation) {
        Write-Host "[OK]" -ForegroundColor $Green -NoNewline
        Write-Host " VCPKG_ROOT directory exists"

        $vcpkgExe = Join-Path $vcpkgLocation "vcpkg.exe"
        if (Test-Path $vcpkgExe) {
            Write-Host "[OK]" -ForegroundColor $Green -NoNewline
            Write-Host " vcpkg.exe found"

            $vcpkgVersion = & $vcpkgExe version 2>&1 | Select-Object -First 1
            Write-Host "  Version: $vcpkgVersion"
            $vcpkgFound = $true
        } else {
            Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
            Write-Host " vcpkg.exe not found in $vcpkgLocation"
            Write-Host "  Run: $vcpkgLocation\bootstrap-vcpkg.bat" -ForegroundColor $Yellow
            $Errors++
        }
    } else {
        Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
        Write-Host " VCPKG_ROOT directory does not exist: $vcpkgLocation"
        $Errors++
    }
} else {
    # Check common vcpkg locations as fallback
    $commonLocations = @(
        "C:\vcpkg",
        "$env:USERPROFILE\.vcpkg-clion\vcpkg",
        "$env:USERPROFILE\vcpkg"
    )

    foreach ($location in $commonLocations) {
        if (Test-Path $location) {
            $vcpkgExe = Join-Path $location "vcpkg.exe"
            if (Test-Path $vcpkgExe) {
                Write-Host "[OK]" -ForegroundColor $Green -NoNewline
                Write-Host " vcpkg found at: $location"
                Write-Host "[WARN]" -ForegroundColor $Yellow -NoNewline
                Write-Host " VCPKG_ROOT not set, but vcpkg detected (likely CLion-managed)"

                $vcpkgVersion = & $vcpkgExe version 2>&1 | Select-Object -First 1
                Write-Host "  Version: $vcpkgVersion"
                Write-Host "  Note: CLion manages this vcpkg automatically" -ForegroundColor $Yellow

                if ($SetEnvVar) {
                    Write-Host ""
                    Write-Host "Setting VCPKG_ROOT environment variable..." -ForegroundColor $Blue
                    [System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', $location, 'User')
                    $env:VCPKG_ROOT = $location
                    Write-Host "[OK]" -ForegroundColor $Green -NoNewline
                    Write-Host " VCPKG_ROOT set to: $location"
                    Write-Host "[WARN]" -ForegroundColor $Yellow -NoNewline
                    Write-Host " Restart your terminal/IDE for VCPKG_ROOT to take effect"
                    # Clear the error since we've set it
                } else {
                    Write-Host "  For command-line builds, set VCPKG_ROOT with:" -ForegroundColor $Yellow
                    Write-Host "    `$env:VCPKG_ROOT = '$location'" -ForegroundColor $Yellow
                    Write-Host "  Or run: .\setup_windows_build_env.ps1 -SetEnvVar" -ForegroundColor $Yellow
                    $Warnings++
                }

                $vcpkgFound = $true
                $vcpkgLocation = $location
                break
            }
        }
    }

    if (-not $vcpkgFound) {
        Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
        Write-Host " VCPKG_ROOT environment variable not set and no vcpkg found"
        if ($Install) {
            Write-Host "  vcpkg will be installed..." -ForegroundColor $Yellow
        } else {
            Write-Host "  Run this script with -Install to set up vcpkg" -ForegroundColor $Yellow
        }
        $Errors++
    }
}

Write-Host ""

# Check CPU
Write-Host "Checking system..." -ForegroundColor $Blue
$cpu = Get-WmiObject -Class Win32_Processor | Select-Object -First 1
Write-Host "[OK]" -ForegroundColor $Green -NoNewline
Write-Host " CPU: $($cpu.Name)"
Write-Host "  Cores: $($cpu.NumberOfCores) / Threads: $($cpu.NumberOfLogicalProcessors)"

Write-Host ""

# Check project structure
Write-Host "Checking project structure..." -ForegroundColor $Blue

if (Test-Path "CMakeLists.txt") {
    Write-Host "[OK]" -ForegroundColor $Green -NoNewline
    Write-Host " CMakeLists.txt found"
} else {
    Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
    Write-Host " CMakeLists.txt not found (run from project root)"
    $Errors++
}

if (Test-Path "vcpkg.json") {
    Write-Host "[OK]" -ForegroundColor $Green -NoNewline
    Write-Host " vcpkg.json found (manifest mode)"
} else {
    Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
    Write-Host " vcpkg.json not found"
    $Errors++
}

if (Test-Path "CMakePresets.json") {
    Write-Host "[OK]" -ForegroundColor $Green -NoNewline
    Write-Host " CMakePresets.json found"
} else {
    Write-Host "[WARN]" -ForegroundColor $Yellow -NoNewline
    Write-Host " CMakePresets.json not found (optional)"
    $Warnings++
}

Write-Host ""

# =============================================================================
# INSTALLATION (if -Install flag is set)
# =============================================================================

if ($Install -and $Errors -gt 0) {
    # Only install vcpkg if that's the missing piece
    # Don't try to install VS, CMake, etc. - those need manual installation

    Write-Host "========================================" -ForegroundColor $Blue
    Write-Host "Installation" -ForegroundColor $Blue
    Write-Host "========================================" -ForegroundColor $Blue
    Write-Host ""

    # Check if only vcpkg is missing
    $onlyVcpkgMissing = (-not $env:VCPKG_ROOT) -and ($cmake -ne $null) -and (Test-Path $vswhere)

    if ($onlyVcpkgMissing) {
        Write-Host "Installing vcpkg..." -ForegroundColor $Blue

        # Determine vcpkg location
        $defaultVcpkgPath = "C:\vcpkg"

        Write-Host "vcpkg will be installed to: $defaultVcpkgPath"
        Write-Host ""

        if (-not (Test-Path $defaultVcpkgPath)) {
            Write-Host "Cloning vcpkg repository..." -ForegroundColor $Blue
            git clone https://github.com/microsoft/vcpkg.git $defaultVcpkgPath
            Write-Host "[OK]" -ForegroundColor $Green -NoNewline
            Write-Host " vcpkg cloned"
        } else {
            Write-Host "[OK]" -ForegroundColor $Green -NoNewline
            Write-Host " vcpkg directory exists"
        }

        # Bootstrap vcpkg
        $vcpkgExe = Join-Path $defaultVcpkgPath "vcpkg.exe"
        if (-not (Test-Path $vcpkgExe)) {
            Write-Host "Bootstrapping vcpkg..." -ForegroundColor $Blue
            & "$defaultVcpkgPath\bootstrap-vcpkg.bat" -disableMetrics
            Write-Host "[OK]" -ForegroundColor $Green -NoNewline
            Write-Host " vcpkg bootstrapped"
        } else {
            Write-Host "[OK]" -ForegroundColor $Green -NoNewline
            Write-Host " vcpkg already bootstrapped"
        }

        # Set environment variable
        Write-Host ""
        Write-Host "Setting VCPKG_ROOT environment variable..." -ForegroundColor $Blue
        [System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', $defaultVcpkgPath, 'User')
        $env:VCPKG_ROOT = $defaultVcpkgPath
        Write-Host "[OK]" -ForegroundColor $Green -NoNewline
        Write-Host " VCPKG_ROOT set to: $defaultVcpkgPath"
        Write-Host ""
        Write-Host "[WARN]" -ForegroundColor $Yellow -NoNewline
        Write-Host " Restart your terminal/IDE for VCPKG_ROOT to take effect"

        $Errors = 0  # vcpkg is now installed
    } else {
        Write-Host "[FAIL]" -ForegroundColor $Red -NoNewline
        Write-Host " Cannot install: prerequisites missing"
        Write-Host ""
        Write-Host "This script can only install vcpkg automatically." -ForegroundColor $Yellow
        Write-Host "Please install missing prerequisites manually:" -ForegroundColor $Yellow
        Write-Host "  - Visual Studio 2019 16.10+ or 2022: https://visualstudio.microsoft.com/" -ForegroundColor $Yellow
        Write-Host "  - CMake 3.16+: https://cmake.org/download/" -ForegroundColor $Yellow
        Write-Host "  - Git: https://git-scm.com/downloads" -ForegroundColor $Yellow
        Write-Host ""
    }
}

# =============================================================================
# SUMMARY
# =============================================================================

Write-Host "========================================" -ForegroundColor $Blue
Write-Host "Validation Summary" -ForegroundColor $Blue
Write-Host "========================================" -ForegroundColor $Blue

if ($Errors -eq 0 -and $Warnings -eq 0) {
    Write-Host "[OK] All checks passed!" -ForegroundColor $Green
    Write-Host "Your build environment is ready." -ForegroundColor $Green
    $exitCode = 0
} elseif ($Errors -eq 0) {
    Write-Host "[WARN] $Warnings warning(s) found" -ForegroundColor $Yellow
    Write-Host "Build should work, but some features may be unavailable." -ForegroundColor $Green
    $exitCode = 0
} else {
    Write-Host "[FAIL] $Errors error(s) found" -ForegroundColor $Red
    if ($Warnings -gt 0) {
        Write-Host "[WARN] $Warnings warning(s) found" -ForegroundColor $Yellow
    }
    Write-Host "Please fix the errors above before building." -ForegroundColor $Red

    if (-not $Install) {
        Write-Host ""
        Write-Host "To install vcpkg automatically, run:" -ForegroundColor $Yellow
        Write-Host "  .\setup_windows_build_env.ps1 -Install" -ForegroundColor $Green
    }
    $exitCode = 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor $Blue
Write-Host "Setup Complete" -ForegroundColor $Blue
Write-Host "========================================" -ForegroundColor $Blue

if ($exitCode -eq 0 -and $Install) {
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor $Yellow
    Write-Host "  1. Restart your terminal/IDE (to load VCPKG_ROOT)" -ForegroundColor $Green
    Write-Host "  2. Configure: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release" -ForegroundColor $Green
    Write-Host "  3. Build: cmake --build build" -ForegroundColor $Green
}

exit $exitCode
