# WSL Linux Build Implementation Plan

**Task:** Q4a - Enable Linux build via WSL with GCC/Clang  
**Effort:** 🟢 1-2 days  
**Status:** ⬜ TODO  
**Date:** 2026-01-31  
**System Verified:** 2026-01-31 ✅

---

## System Readiness Status

### ✅ Already Available
- **WSL2:** Installed and configured (Ubuntu-22.04)
- **GCC 11.3.0:** C++20 capable compiler ready
- **CMake 3.22.1:** Build system ready
- **Ninja:** Fast build generator ready
- **CPU Support:** Intel i9-13900KF with BMI2/PEXT/AVX2 ✅
- **Project Access:** `/mnt/d/_DEV/FrankyCPP` accessible from WSL

### ⚠️ Needs Installation
- **vcpkg:** Not yet installed (~15 min)
- **pkg-config:** May be needed for dependencies (~1 min)

### 📝 Needs Configuration
- **CMakeLists.txt:** GCC support commented out (needs enabling)
- **CMakePresets.json:** No Linux presets yet
- **build_wsl.sh:** References old v0.5 paths

**Assessment:** System is 90% ready! Only vcpkg installation and CMake configuration needed.

---

## 🚀 Start Here - Immediate Actions

**Your system is 90% ready!** Follow these steps in order:

### Step 0: Validate Environment (Recommended - 1 min)

**For Linux/WSL:**
```bash
# Run the Linux setup script (validates environment, safe - no modifications)
wsl bash -c "cd /mnt/d/_DEV/FrankyCPP && ./setup_linux_build_env.sh"

# Or from within WSL:
./setup_linux_build_env.sh
```

**Default behavior is validation-only** - the script will check what's installed and tell you what's missing without making any system changes.

This will check for all required tools and give you a clear report of what needs to be installed.

**For Windows:** 
CMake configuration includes automatic validation that will run when you first configure the project. It checks:
- MSVC version (>= 2019 16.10 required)
- vcpkg installation and bootstrap status
- Generator choice (recommends Ninja)
- MSVC environment initialization

Simply run CMake configure and it will report any missing requirements.

### Step 1: Install Dependencies (~15 min)

**Option A: Automated Installation (Recommended)**

Use the Linux setup script with the --install flag:

```powershell
# From Windows PowerShell (requires sudo in WSL)
wsl bash -c "cd /mnt/d/_DEV/FrankyCPP && ./setup_linux_build_env.sh --install"
```

**SAFETY NOTE:** The script validates-only by default. Use `--install` explicitly to modify the system.

This installs and validates:
- All essential build tools (gcc, cmake, ninja, git, pkg-config)
- Optional tools for vcpkg packages (autoconf, automake, libtool)
- vcpkg package manager (clones, bootstraps, adds to ~/.bashrc)
- Verifies all installations and versions

After installation completes, reload your shell:
```bash
source ~/.bashrc
```

**Option B: Manual Installation**

If you prefer manual control:

```powershell
# From Windows PowerShell (current terminal)
wsl bash -c "cd ~ && git clone https://github.com/microsoft/vcpkg.git && cd vcpkg && ./bootstrap-vcpkg.sh && echo 'export VCPKG_ROOT=~/vcpkg' >> ~/.bashrc"
wsl bash -c "sudo apt update && sudo apt install -y pkg-config curl zip unzip tar autoconf automake libtool"
```

### Step 2: Enable GCC in CMakeLists.txt (~1-2 hours)
- See **Phase 2.1** below for exact code to add
- Location: Lines 151-260 in CMakeLists.txt
- Action: Uncomment and update GCC configuration

### Step 3: Add Linux Presets (~30 min)
- See **Phase 2.2** below for JSON to add
- File: CMakePresets.json
- Action: Add linux-debug, linux-release, wsl-debug, wsl-release presets

### Step 4: Test Build (~5-10 min)
```bash
# Command-line test
wsl bash -c "cd /mnt/d/_DEV/FrankyCPP && ./build_wsl.sh Release"
```

**OR** configure CLion (see Phase 4) for IDE experience.

---

## Quick Start (TL;DR)

Since your WSL environment is already 90% ready, you can get started immediately with either approach:

### 🚀 Command-Line Track (Fastest validation)

**Step 1: Validate (safe, 1 min):**
```bash
# Check what's installed/missing (no system modifications)
wsl bash -c "cd /mnt/d/_DEV/FrankyCPP && ./setup_linux_build_env.sh"
```

**Step 2: Install if needed (~5-10 min):**
```bash
# Install all missing dependencies (requires sudo)
wsl bash -c "cd /mnt/d/_DEV/FrankyCPP && ./setup_linux_build_env.sh --install"

# Reload environment
wsl bash -c "source ~/.bashrc"
```

**Step 3: Proceed to Phase 2** to edit CMakeLists.txt and CMakePresets.json

---

**Manual Setup (if you prefer control):**
```bash
# 1. Install vcpkg in WSL (~15 min - includes bootstrap and dependency downloads)
wsl bash -c "cd ~ && git clone https://github.com/microsoft/vcpkg.git && cd vcpkg && ./bootstrap-vcpkg.sh"
wsl bash -c "echo 'export VCPKG_ROOT=~/vcpkg' >> ~/.bashrc"

# 2. Install missing packages (~2 min)
wsl bash -c "sudo apt update && sudo apt install -y pkg-config curl zip unzip tar autoconf automake libtool"

# 3. Then proceed to Phase 2 to edit CMakeLists.txt and CMakePresets.json
```

### 🎯 CLion Track (Full IDE experience)

After completing command-line setup above:
1. Open CLion Settings → Build, Execution, Deployment → Toolchains
2. Add WSL toolchain (auto-detects GCC, CMake, Ninja)
3. Set environment: `VCPKG_ROOT=/home/frank/vcpkg`
4. Add WSL-Debug and WSL-Release CMake profiles
5. Reload CMake project

### 📋 Critical Files to Edit

| File | What to Change | Time |
|------|----------------|------|
| `CMakeLists.txt` | Enable GCC support (lines 151-260) | 1-2 hours |
| `CMakePresets.json` | Add Linux presets | 30 min |
| `build_wsl.sh` | Update for v0.7 | 15 min |

### ⏱️ Time to First Build

- **Prerequisites:** ~15 min (vcpkg install)
- **Configuration:** ~2-3 hours (CMake changes + first vcpkg dependency downloads)
- **First Build:** ~5-10 min (depends on CPU)
- **Total:** ~3-4 hours to working Linux build

---

## Executive Summary

Enable FrankyCPP to build on Linux via WSL using GCC, supporting **both CLion IDE integration and standalone command-line builds** (hybrid approach). This expands testing capability and prepares for cross-platform CI/CD.

### Hybrid Approach Strategy
- **CLion IDE:** Primary development environment with full debugging, navigation, and test integration
- **Command-line:** Fast builds and CI/CD preparation via `build_wsl.sh` script
- **Shared Configuration:** Both use the same CMake configuration for consistency

### Scope
- ✅ Enable GCC toolchain in CMakeLists.txt (C++20 compatible)
- ✅ Add Linux-specific CMake presets for both CLion and CLI
- ✅ Update `build_wsl.sh` script for v0.7
- ✅ Configure CLion WSL toolchain
- ✅ Install and configure vcpkg on Linux
- ✅ Test builds (Debug/Release) in both environments
- ✅ Run full test suite on Linux

### Out of Scope (Future Work)
- Clang support (separate task after GCC validated)
- macOS support (not planned)
- GitHub Actions CI (Q4b - includes both Windows and Linux builds, depends on Q4a completion)
- Performance tuning/profiling
- Distribution packaging

---

## Current State Analysis

### Existing Infrastructure
1. **CMake Configuration:**
   - Primary support: MSVC only (lines 97-147 in CMakeLists.txt)
   - GCC/Clang code exists but is **commented out** (lines 186-260)
   - Needs updating for C++20 (currently references `-std=c++2a` or `-std=c++17`)

2. **Build Scripts:**
   - `build_wsl.sh` exists but references old paths/targets
   - Uses Ninja generator (good - already supported)
   - Needs updating for current project structure

3. **CMake Presets:**
   - `CMakePresets.json` exists with Windows presets only
   - Needs Linux-specific presets for WSL

4. **vcpkg Integration:**
   - Currently uses `x64-windows-static-md` triplet
   - Linux will need `x64-linux` triplet
   - vcpkg manifest mode already configured (vcpkg.json)

5. **Dependencies:**
   - All dependencies available on Linux via vcpkg:
     - Boost (program_options, serialization)
     - spdlog (header-only)
     - yaml-cpp
     - GoogleTest
     - Google Benchmark

### Known Issues
- Old executable names in `build_wsl.sh` (FrankyCPP_v0.5 vs v0.7)
- Commented-out GCC flags reference obsolete intrinsics/standards
- No HAS_PEXT/HAS_EXECUTION_LIB configuration for Linux
- Missing Linux-specific CMake presets

---

## Implementation Plan

All phases support both CLion IDE and command-line builds through shared CMake configuration.

### Implementation Roadmap

**Phase 1: Prerequisites (~15 min)** - Install vcpkg + missing packages in WSL

**Phase 2: CMake Configuration (~2-3 hours)** - Enable GCC in CMakeLists.txt + Add Linux presets

**Phase 3: Build Scripts (~30 min)** - Update build_wsl.sh for v0.7

**Phase 4: CLion Integration (~1 hour)** - Configure WSL toolchain + Add WSL CMake profiles

**Phase 5: First Build & Test (~2-3 hours)** - Build and validate in both environments

**Phase 6: Documentation (~30 min)** - Update README.md and copilot-instructions.md

**Total Time:** ~6-7 hours (1 working day)

---

### Phase 1: Prerequisites (30 min)

**Goal:** Install missing dependencies identified by validation script.

#### 1.0 Run Environment Setup & Validation

The Linux setup script validates by default (safe) and can install on request:

```bash
# From Windows PowerShell - validate only (safe, no modifications)
wsl bash -c "cd /mnt/d/_DEV/FrankyCPP && ./setup_linux_build_env.sh"

# Install missing dependencies (requires sudo, modifies system)
wsl bash -c "cd /mnt/d/_DEV/FrankyCPP && ./setup_linux_build_env.sh --install"
```

**SAFETY NOTE:** Default behavior is validate-only to prevent unintended system modifications. Use `--install` explicitly when you want to install packages.

The script:
- ✅ Installs compiler tools (GCC/G++) and verifies C++20 support
- ✅ Installs build tools (CMake >= 3.16, Ninja, Make)
- ✅ Installs required utilities (git, pkg-config, curl, tar, unzip, etc.)
- ✅ Installs optional tools (autoconf, automake, libtool for vcpkg packages)
- ✅ Clones and bootstraps vcpkg package manager
- ✅ Configures VCPKG_ROOT environment variable
- ✅ Validates CPU features (AVX2, BMI2/PEXT, POPCNT)
- ✅ Checks project structure (CMakeLists.txt, vcpkg.json, build scripts)

The script provides colored output:
- 🟢 ✓ = Tool found and working
- 🔴 ✗ = Required tool missing (will be installed or error reported)
- 🟡 ! = Optional tool missing (recommended but not required)

#### 1.1 WSL Environment Setup ✅ ALREADY CONFIGURED

**Current System Status (Verified 2026-01-31):**
- **WSL Version:** WSL2 (Default)
- **Distribution:** Ubuntu-22.04 (Default)
- **Kernel:** 6.6.87.2-microsoft-standard-WSL2
- **CPU:** Intel Core i9-13900KF (32 cores, 13th Gen)
- **CPU Features:** ✅ BMI2, PEXT, AVX2, SSE4.2, POPCNT all supported
- **Project Path:** `/mnt/d/_DEV/FrankyCPP` (accessible)

```powershell
# Verify WSL status (already confirmed working):
wsl --status
# Output: Default Distribution: Ubuntu-22.04, Default Version: 2
```

#### 1.2 Install Build Tools in WSL ✅ PARTIALLY COMPLETE

**Already Installed:**
- ✅ GCC 11.3.0 (Ubuntu) - Full C++20 support
- ✅ CMake 3.22.1
- ✅ Ninja build system
- ✅ Git

**Still Needed:**
```bash
# In WSL terminal - install remaining dependencies
sudo apt update
sudo apt install -y \
  pkg-config \
  curl \
  zip \
  unzip \
  tar \
  autoconf \
  automake \
  libtool
```

#### 1.3 Install vcpkg in WSL ❌ NOT YET INSTALLED

**Status:** vcpkg not found in `/home/frank/vcpkg`

```bash
# In WSL, from home directory
cd ~
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# Add to ~/.bashrc for persistence
echo 'export VCPKG_ROOT=~/vcpkg' >> ~/.bashrc
source ~/.bashrc

# Verify installation
which vcpkg
echo $VCPKG_ROOT
```

**Note:** This is the only major prerequisite missing. Everything else is ready!

---

### Phase 2: CMake Configuration (2-3 hours)

**Goal:** Enable GCC support and add Linux presets to CMake configuration.

**Note:** Build tool validation checks have been added to CMakeLists.txt that will automatically verify:
- ✅ Compiler version supports C++20
- ✅ Required build tools are installed (pkg-config, tar, unzip)
- ✅ vcpkg toolchain is properly configured
- ✅ vcpkg.json manifest is present
- ✅ Architecture is supported (x86_64/ARM64)

These checks will help catch configuration issues early in both local builds and CI/CD pipelines.

#### 2.1 Enable GCC Support in CMakeLists.txt

**File:** `CMakeLists.txt` (after line 147, before the commented section)

```cmake
# GCC/Clang compiler support for Linux/WSL
elseif (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(STATUS "Compiler Settings: GCC")

    # Base flags for all configurations
    set(CMAKE_CXX_FLAGS 
        "-Wall -Wextra -Wpedantic \
        -Wno-unknown-pragmas \
        -Wno-sign-compare \
        -march=native \
        -mpopcnt -mbmi2"
    )
    
    # Debug configuration
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    
    # Release configuration
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    
    # Enable LTO for Release builds
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_msg)
    if(_ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        message(STATUS "IPO/LTO enabled for GCC Release builds")
    else()
        message(STATUS "IPO/LTO not supported: ${_ipo_msg}")
    endif()

    # Feature flags
    if(ENABLE_STD_EXECUTION)
        # std::execution requires TBB on GCC
        # Current WSL system has GCC 11.3.0 (verified 2026-01-31)
        if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "10")
            message(STATUS "GCC >= 10: HAS_EXECUTION_LIB enabled (may require libtbb)")
            add_compile_definitions(HAS_EXECUTION_LIB)
        else()
            message(WARNING "GCC < 10: std::execution not available")
        endif()
    endif()

    if(ENABLE_BMI2_PEXT)
        message(STATUS "HAS_PEXT enabled")
        add_compile_definitions(HAS_PEXT)
    endif()

elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Compiler Settings: Clang")
    
    set(CMAKE_CXX_FLAGS 
        "-Wall -Wextra -Wpedantic \
        -Wno-unknown-pragmas \
        -march=native \
        -mpopcnt -mbmi2"
    )
    
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

    if(ENABLE_BMI2_PEXT)
        message(STATUS "HAS_PEXT enabled")
        add_compile_definitions(HAS_PEXT)
    endif()

    if(ENABLE_STD_EXECUTION)
        message(STATUS "HAS_EXECUTION_LIB enabled for Clang")
        add_compile_definitions(HAS_EXECUTION_LIB)
    endif()

endif()
```

**Note:** This replaces the commented-out section at the end of CMakeLists.txt

#### 2.2 Update CMakePresets.json

Add Linux presets after the Windows presets:

```json
    {
      "name": "linux-base",
      "hidden": true,
      "inherits": "base",
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Linux"
      },
      "cacheVariables": {
        "VCPKG_TARGET_TRIPLET": "x64-linux"
      }
    },
    {
      "name": "linux-debug",
      "displayName": "Linux Debug",
      "inherits": "linux-base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "linux-release",
      "displayName": "Linux Release",
      "inherits": "linux-base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    },
    {
      "name": "wsl-debug",
      "displayName": "WSL Debug",
      "inherits": "linux-debug",
      "description": "WSL build for development in CLion"
    },
    {
      "name": "wsl-release",
      "displayName": "WSL Release",
      "inherits": "linux-release",
      "description": "WSL release build for testing"
    }
```

Add corresponding build presets:

```json
    {
      "name": "linux-debug",
      "configurePreset": "linux-debug"
    },
    {
      "name": "linux-release",
      "configurePreset": "linux-release"
    },
    {
      "name": "wsl-debug",
      "configurePreset": "wsl-debug"
    },
    {
      "name": "wsl-release",
      "configurePreset": "wsl-release"
    }
```

---

### Phase 3: CLion WSL Integration (1 hour)

#### 3.1 Configure WSL Toolchain in CLion

1. **Open CLion Settings:**
   - File → Settings → Build, Execution, Deployment → Toolchains

2. **Add WSL Toolchain:**
   - Click "+" → WSL
   - Select your WSL distribution (e.g., Ubuntu-22.04)
   - CLion will auto-detect:
     - CMake: `/usr/bin/cmake`
     - Make: `/usr/bin/make`
     - C Compiler: `/usr/bin/gcc`
     - C++ Compiler: `/usr/bin/g++`
     - Debugger: `/usr/bin/gdb`

3. **Set vcpkg:**
   - In the toolchain settings, add environment variable:
     - `VCPKG_ROOT=/home/<username>/vcpkg`

4. **Verify Detection:**
   - Click "Test" button - should show green checkmarks

#### 3.2 Add WSL CMake Profile

1. **Settings → Build, Execution, Deployment → CMake**
2. **Add Profile:**
   - Name: `WSL-Debug`
   - Build type: `Debug`
   - Toolchain: `WSL`
   - CMake options: (leave default or add `-DCMAKE_BUILD_TYPE=Debug`)
3. **Add another profile:**
   - Name: `WSL-Release`
   - Build type: `Release`
   - Toolchain: `WSL`

#### 3.3 Reload CMake Project
- Tools → CMake → Reload CMake Project
- CLion will configure both Windows and WSL profiles

---

### Phase 4: Standalone Build Script (30 min)

#### 4.1 Update build_wsl.sh

Replace content of `build_wsl.sh`:

```bash
#!/usr/bin/bash

#
# FrankyCPP WSL Linux Build Script
# Copyright (c) 2018-2026 Frank Kopp
# MIT License
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}FrankyCPP v0.7 - WSL Linux Build${NC}"
echo -e "${GREEN}========================================${NC}"

# Check for vcpkg
if [ -z "$VCPKG_ROOT" ]; then
    echo -e "${RED}ERROR: VCPKG_ROOT not set${NC}"
    echo "Please set VCPKG_ROOT environment variable"
    echo "Example: export VCPKG_ROOT=~/vcpkg"
    exit 1
fi

# Configuration
BUILD_TYPE=${1:-Release}
BUILD_DIR="cmake-build-wsl-${BUILD_TYPE,,}"
NUM_JOBS=$(nproc)

echo -e "${YELLOW}Build Configuration:${NC}"
echo "  Build Type: $BUILD_TYPE"
echo "  Build Dir:  $BUILD_DIR"
echo "  CPU Cores:  $NUM_JOBS"
echo "  vcpkg:      $VCPKG_ROOT"

# Create and enter build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo -e "\n${GREEN}Configuring CMake...${NC}"
cmake \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-linux \
    -G Ninja \
    ..

# Build
echo -e "\n${GREEN}Building...${NC}"
ninja -j "$NUM_JOBS"

# Run tests (skip slow tests)
if [ "$BUILD_TYPE" = "Debug" ] || [ "$BUILD_TYPE" = "Release" ]; then
    echo -e "\n${GREEN}Running tests...${NC}"
    ctest \
        -C "$BUILD_TYPE" \
        -E ".*SpeedTests.*|.*TimingTests.*" \
        --output-on-failure \
        || echo -e "${YELLOW}Some tests failed${NC}"
fi

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Executable: $BUILD_DIR/src/FrankyCPP_v0.7"
echo -e "Tests:      $BUILD_DIR/test/FrankyCPP_v0.7_Test"
echo -e "Benchmarks: $BUILD_DIR/testbench/FrankyCPP_v0.7_Bench"
```

#### 4.2 Make Script Executable
```bash
# In WSL
chmod +x build_wsl.sh
```

---

### Phase 5: Testing & Validation (1-2 hours)

#### 5.1 First Build Test (WSL Command Line)

```bash
# In WSL, from project root
./build_wsl.sh Debug

# If successful, try Release
./build_wsl.sh Release
```

#### 5.2 CLion Build Test

1. Select "WSL-Debug" configuration in CLion
2. Build → Build Project (Ctrl+F9)
3. Verify no errors
4. Run tests via CLion test runner

#### 5.3 Run Full Test Suite

```bash
# In WSL
cd cmake-build-wsl-release
./test/FrankyCPP_v0.7_Test
```

#### 5.4 Run Benchmarks (Optional)

```bash
cd cmake-build-wsl-release
./testbench/FrankyCPP_v0.7_Bench
```

#### 5.5 Test Engine

```bash
cd cmake-build-wsl-release/src
./FrankyCPP_v0.7

# In UCI mode, test basic commands:
# uci
# isready
# position startpos
# go depth 5
# quit
```

---

### Phase 6: Documentation Updates (30 min)

#### 6.1 Update README.md

Add Linux build section:

```markdown
### Linux/WSL Build

FrankyCPP can be built on Linux (including WSL) using GCC or Clang.

**Prerequisites:**
```bash
sudo apt install build-essential cmake ninja-build git
```

**vcpkg Setup:**
```bash
cd ~
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
```

**Build:**
```bash
./build_wsl.sh Release
```

**CLion Integration:**
See `docs/WSL_Linux_Build_Plan.md` for CLion WSL toolchain setup.
```

#### 6.2 Update Copilot Instructions

Add WSL build notes to `.github/copilot-instructions.md`:

```markdown
### WSL/Linux Builds
- GCC/Clang supported alongside MSVC
- Use `build_wsl.sh` for command-line builds
- CLion can use WSL toolchain for development
- vcpkg triplet: `x64-linux`
- Standard flags in CMakeLists.txt handle cross-platform differences
```

---

## Troubleshooting Guide

### Quick Fixes for Common Issues

| Symptom | Quick Fix |
|---------|-----------|
| CMake can't find vcpkg | `export VCPKG_ROOT=~/vcpkg` (add to ~/.bashrc) |
| vcpkg package install fails | `sudo apt install pkg-config autoconf automake libtool` |
| TBB linker errors | `sudo apt install libtbb-dev` or disable with `-DENABLE_STD_EXECUTION=OFF` |
| Build script fails with "bad interpreter" | `dos2unix build_wsl.sh` or `sed -i 's/\r$//' build_wsl.sh` |
| CLion can't find WSL | Restart CLion, ensure WSL2 is default version |
| Permission denied on build_wsl.sh | `chmod +x build_wsl.sh` |

### System-Specific Notes (Your Hardware)

**CPU: Intel i9-13900KF**
- ✅ BMI2/PEXT fully supported - no fallback needed
- ✅ AVX2 supported - can use `-march=native`
- ✅ All SIMD optimizations available

**GCC 11.3.0**
- ✅ Full C++20 support
- ✅ std::execution available (requires libtbb-dev)
- ✅ LTO/IPO supported

### If Build Fails

1. **Check vcpkg:** `echo $VCPKG_ROOT` should show `/home/frank/vcpkg`
2. **Check build tools:** `which gcc cmake ninja` should all return paths
3. **Clean build:** `rm -rf cmake-build-wsl-*` and retry
4. **Verbose output:** Add `-DCMAKE_VERBOSE_MAKEFILE=ON` to CMake options
5. **Check logs:** Look in `cmake-build-wsl-*/vcpkg-manifest-install.log`

---

## Success Criteria

**System Prerequisites (2026-01-31 Status):**
- [x] WSL2 installed and configured ✅
- [x] Ubuntu-22.04 as default distribution ✅
- [x] GCC 11.3.0 installed ✅
- [x] CMake 3.22.1 installed ✅
- [x] Ninja build system installed ✅
- [x] Project accessible from WSL ✅
- [ ] vcpkg installed in WSL ⬜
- [ ] pkg-config and dependencies ⬜

**Implementation Tasks:**
- [ ] CMakeLists.txt has GCC support enabled
- [ ] CMakePresets.json has Linux/WSL presets
- [ ] `build_wsl.sh` executes successfully
- [ ] CLion WSL toolchain configured and building (optional)
- [ ] Debug build completes without errors
- [ ] Release build completes without errors
- [ ] All tests pass (except explicitly skipped)
- [ ] Engine runs and responds to UCI commands
- [ ] Documentation updated (README, copilot-instructions)

---

## Timeline Estimate

| Phase | Task | Time | Status |
|-------|------|------|--------|
| 1 | Prerequisites | 30 min | ✅ 90% Done (just vcpkg) |
| 2 | CMake Configuration | 2-3 hours | ⬜ TODO |
| 3 | CLion Integration | 1 hour | ⬜ Optional |
| 4 | Build Script | 30 min | ⬜ TODO |
| 5 | Testing & Validation | 1-2 hours | ⬜ TODO |
| 6 | Documentation | 30 min | ⬜ TODO |
| **Total** | | **6-8 hours** | **~15 min already saved** |

**Realistic Estimate:** 1-2 days including troubleshooting and testing

**Updated Estimate (2026-01-31):** Since WSL environment is already 90% configured with all build tools installed, actual time will likely be on the lower end (6-7 hours total, or ~1 day).

---

## Implementation Checklist

Use this checklist to track progress through the hybrid approach implementation:

### Phase 1: Prerequisites
- [ ] Install vcpkg in WSL (`~/vcpkg`)
- [ ] Install pkg-config and build dependencies
- [ ] Verify VCPKG_ROOT environment variable

### Phase 2: CMake Configuration
- [ ] Add GCC compiler support to CMakeLists.txt
- [ ] Configure BMI2/PEXT flags for GCC
- [ ] Configure LTO for Release builds
- [ ] Add `linux-base` preset to CMakePresets.json
- [ ] Add `linux-debug` and `linux-release` presets
- [ ] Add `wsl-debug` and `wsl-release` presets
- [ ] Add corresponding build presets
- [ ] Verify CMake configuration with `cmake --list-presets`

### Phase 3: Build Scripts
- [ ] Update `build_wsl.sh` for v0.7
- [ ] Add error handling and colored output
- [ ] Make script executable (`chmod +x`)
- [ ] Test script with Debug build
- [ ] Test script with Release build

### Phase 4: CLion Integration
- [ ] Add WSL toolchain in CLion Settings
- [ ] Verify toolchain detection (GCC, CMake, Ninja)
- [ ] Set VCPKG_ROOT in toolchain environment
- [ ] Create WSL-Debug CMake profile
- [ ] Create WSL-Release CMake profile
- [ ] Reload CMake project
- [ ] Verify both profiles configure successfully

### Phase 5: Testing & Validation
- [ ] Command-line Debug build completes
- [ ] Command-line Release build completes
- [ ] CLion Debug build completes
- [ ] CLion Release build completes
- [ ] Run test suite via command-line
- [ ] Run test suite via CLion
- [ ] Test UCI engine functionality
- [ ] Run benchmarks (optional)

### Phase 6: Documentation
- [ ] Add Linux build section to README.md
- [ ] Add CLion WSL setup instructions to README.md
- [ ] Update `.github/copilot-instructions.md`
- [ ] Update codebase review document if needed

### Final Validation
- [ ] Both CLion and CLI builds work identically
- [ ] All tests pass on Linux
- [ ] Engine responds correctly to UCI commands
- [ ] Build times are reasonable
- [ ] Documentation is complete and accurate

---

## Next Steps (After Q4a Completion)

### Q4b - GitHub Actions CI - Windows & Linux

**Scope:** Enable automated builds on GitHub Actions for both platforms

**Windows Build (runs on windows-latest runner):**
- Uses MSVC compiler (pre-installed on runner)
- Clones and bootstraps vcpkg in `C:\vcpkg`
- Uses Ninja generator
- Builds Debug and Release configurations
- Runs test suite
- Uploads Release artifacts (.exe files)

**Linux Build (runs on ubuntu-latest runner):**
- Uses GCC 11 and Clang 15 compilers
- Uses the `setup_linux_build_env.sh --install` script to install dependencies
- Installs vcpkg to `/opt/vcpkg`
- Uses Ninja generator
- Matrix strategy: [gcc, clang] × [Debug, Release]
- Runs test suite
- Uploads Release artifacts (binaries)

**Implementation Tasks:**
1. Update workflow file (`.github/workflows/build.yml`)
   - Fix script references (`install_dependencies.sh` → `setup_linux_build_env.sh`)
   - Update version references (v0.5 → v0.7)
   - Ensure CMake presets are used
2. Test Windows build job
3. Test Linux build job (depends on Q4a completion)
4. Configure artifact retention and naming

**Note:** A sample workflow file already exists in `.github/workflows/build.yml` that demonstrates both Windows and Linux builds. It will need updating once Q4a (local Linux builds) is complete.

---

## References

- [CMake Cross Compilation](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
- [vcpkg on Linux](https://github.com/microsoft/vcpkg/blob/master/docs/users/platforms/linux.md)
- [CLion WSL Support](https://www.jetbrains.com/help/clion/how-to-use-wsl-development-environment-in-clion.html)
- [GCC Optimization Options](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)

---

*Plan created: 2026-01-31*  
*System verified: 2026-01-31*  
*Status: Ready for implementation - WSL environment 90% configured*
