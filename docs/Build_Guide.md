# FrankyCPP Build Guide

**Version:** 1.3  
**Last Updated:** 2026-02-16  
**Status:** All platforms working ✅

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Supported Platforms](#supported-platforms)
3. [Prerequisites](#prerequisites)
4. [Building on Windows](#building-on-windows)
5. [Building on Linux/WSL](#building-on-linuxwsl)
6. [Cross-Compiler Testing with Clang](#cross-compiler-testing-with-clang)
7. [Syzygy Tablebase Setup (Optional)](#syzygy-tablebase-setup-optional)
8. [CMake Presets](#cmake-presets)
9. [CI/CD Pipeline](#cicd-pipeline)
10. [Release Packaging](#release-packaging)
11. [Troubleshooting](#troubleshooting)
12. [Technical Details](#technical-details)

---

## Quick Start

### Windows (MSVC)
```powershell
# One-time setup
.\setup_windows_build_env.ps1

# Build and test
.\build_windows.ps1 release
```

### Linux/WSL (GCC)
```bash
# One-time setup
./setup_linux_build_env.sh --install

# Build and test
./build_wsl.sh release gcc
```

### Linux/WSL (Clang)
```bash
# Build and test with Clang 18
./build_wsl.sh release clang
```

---

## Supported Platforms

| Platform      | Compiler | Version     | C++20 Support | Status   |
|---------------|----------|-------------|---------------|----------|
| **Windows**   | MSVC     | 2022 (17.x) | ✅ Full        | ✅ Tested |
| **Linux/WSL** | GCC      | 13.0+       | ✅ Full        | ✅ Tested |
| **Linux/WSL** | Clang    | 18.0+       | ✅ Full        | ✅ Tested |

### Test Results
- Windows (MSVC 2022): ✅ All 266 tests passing
- Linux (GCC 13): ✅ All 266 tests passing
- Linux (Clang 18): ✅ All tests passing

---

## Prerequisites

### Windows
- **Visual Studio 2022** (or 2019 16.10+)
  - Desktop development with C++ workload
  - Windows 10 SDK
- **CMake** 3.22+ (included with VS or from cmake.org)
- **vcpkg** (auto-setup via script)
- **Git** for Windows

### Linux/WSL
- **Ubuntu 22.04 or 24.04** (or compatible distro)
- **GCC 13+** (for libstdc++ with std::format)
- **Clang 18+** (optional, for cross-compiler testing)
- **CMake 3.22+**
- **Ninja** build system
- **vcpkg** (auto-setup via script)
- **Git**

### Common Dependencies (via vcpkg)
- Boost (program_options, serialization)
- spdlog (header-only logging)
- yaml-cpp (configuration)
- GoogleTest (testing framework)
- Google Benchmark (performance testing)

---

## Building on Windows

### Initial Setup

```powershell
# Validate environment (or install vcpkg if needed)
.\setup_windows_build_env.ps1

# With -Install flag to set up vcpkg
.\setup_windows_build_env.ps1 -Install
```

The script will:
- ✅ Check Visual Studio version (>= 2019 16.10)
- ✅ Verify CMake, Ninja, Git
- ✅ Clone and bootstrap vcpkg (if -Install)
- ✅ Set VCPKG_ROOT environment variable

### Build Commands

```powershell
# Release build (recommended)
.\build_windows.ps1 release

# Debug build
.\build_windows.ps1 debug

### Other configurations
.\build_windows.ps1 relwithdebinfo
.\build_windows.ps1 minsizerel

# Production Build (Static Constexpr Configs)
# Note: Must use cmake directly as build script doesn't support custom presets yet
cmake --preset win-release-production
cmake --build cmake-build-win-release-production --config Release

# Show help
.\build_windows.ps1 -Help
```

### Build Output
- Executables: `cmake-build-win-release\src\FrankyCPP_v0.7.exe`
- Tests: `cmake-build-win-release\test\FrankyCPP_v0.7_Test.exe`

### Running Tests
```powershell
# Run all tests (excluding slow tests)
.\cmake-build-win-release\test\FrankyCPP_v0.7_Test.exe --gtest_filter=-*SpeedTests.*:*TimingTests.*

# Run specific test
.\cmake-build-win-release\test\FrankyCPP_v0.7_Test.exe --gtest_filter=PositionTest.*
```

---

## Building on Linux/WSL

### Initial Setup

```bash
# Install all build tools and compilers
./setup_linux_build_env.sh --install

# Validate only (no installation)
./setup_linux_build_env.sh
```

The script will:
- ✅ Install GCC 13 and set as default
- ✅ Install Clang 18 from LLVM repository
- ✅ Install CMake, Ninja, and build tools
- ✅ Clone and bootstrap vcpkg
- ✅ Configure VCPKG_ROOT in ~/.bashrc

### Build with GCC (Primary)

```bash
# Release build (recommended)
./build_wsl.sh release gcc

# Debug build
./build_wsl.sh debug gcc

# Shorthand (defaults to GCC)
./build_wsl.sh release
./build_wsl.sh debug

# Production Build (Static Constexpr Configs)
# Note: Manually invoke cmake to use production preset
# This builds with aggressive optimizations and stripped features
cmake --preset wsl-release-production
cmake --build cmake-build-wsl-release-production
```

### Build Output
- Executables: `cmake-build-wsl-release/src/FrankyCPP_v0.7`
- Tests: `cmake-build-wsl-release/test/FrankyCPP_v0.7_Test`

### Running Tests
```bash
# Run all tests (excluding slow tests)
./cmake-build-wsl-release/test/FrankyCPP_v0.7_Test --gtest_filter=-*SpeedTests.*:*TimingTests.*

# Run specific test suite
./cmake-build-wsl-release/test/FrankyCPP_v0.7_Test --gtest_filter=PositionTest.*
```

---

## Cross-Compiler Testing with Clang

### Why Test with Clang?

1. **Catch compiler-specific issues** - Different warning sets and optimizations
2. **Validate standards compliance** - Clang is stricter about C++20
3. **Prepare for macOS** - macOS uses Clang as primary compiler
4. **CI alignment** - Match GitHub Actions test matrix

### Building with Clang 18

```bash
# Release build with Clang
./build_wsl.sh release clang

# Debug build with Clang
./build_wsl.sh debug clang
```

### Clang + libstdc++ Configuration

**Important:** Clang 18 on Linux uses GCC's libstdc++ for compatibility with vcpkg libraries.

**Why?**
- vcpkg libraries are compiled with GCC and use libstdc++
- Mixing standard libraries (libc++ and libstdc++) causes linking errors
- libstdc++ from GCC 13 has full C++20 support including std::format

**Automatic Configuration:**
```cmake
# CMakeLists.txt automatically configures this on Linux
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND UNIX)
    add_compile_options(-stdlib=libstdc++)
    add_link_options(-stdlib=libstdc++)
endif()
```

### Clang 18 vs Clang 15

We upgraded from Clang 15 to Clang 18 because:
- ✅ Better C++20 ranges/concepts compatibility with libstdc++
- ✅ No precompiled header issues
- ✅ Better diagnostics and error messages
- ✅ More complete C++20 support

### Build Output
- Executables: `cmake-build-wsl-clang-release/src/FrankyCPP_v0.7`
- Tests: `cmake-build-wsl-clang-release/test/FrankyCPP_v0.7_Test`

### Testing Workflow

```bash
# Test with both compilers before pushing
./build_wsl.sh release gcc     # Primary compiler
./build_wsl.sh release clang   # Cross-compiler validation

# Both should succeed! ✅
```

---

## Syzygy Tablebase Setup (Optional)

Syzygy tablebases provide perfect endgame play for positions with few pieces. FrankyCPP includes a built-in downloader for easy setup.

### Tablebase Sizes

| Pieces  | Size    | Recommended                    |
|---------|---------|--------------------------------|
| 3-piece | ~7 MB   | ✅ Yes                          |
| 4-piece | ~75 MB  | ✅ Yes                          |
| 5-piece | ~1 GB   | ✅ Yes                          |
| 6-piece | ~150 GB | ⚠️ Large (torrent recommended) |

### Quick Setup (3-4-5 piece)

```bash
# Windows
.\cmake-build-win-release\src\FrankyCPP_v0.7.exe --syzygy download --pieces 3-4-5 --path D:\Chess\Syzygy

# Linux/WSL
./cmake-build-wsl-release/src/FrankyCPP_v0.7 --syzygy download --pieces 3-4-5 --path ~/syzygy
```

### Check Status

```bash
# Check configured tablebase status
FrankyCPP --syzygy status

# Check specific directory
FrankyCPP --syzygy status --path D:\Chess\Syzygy
```

### Configuration

After downloading, configure the path in one of these ways:

**Option 1: UCI option (per session)**
```
setoption name SyzygyPath value D:\Chess\Syzygy
```

**Option 2: YAML config (persistent)**
```yaml
# config/search.yaml
TB_PATH: "D:\\Chess\\Syzygy"
```

**Option 3: Environment variable**
```bash
# Linux
export TB_PATH=~/syzygy

# Windows PowerShell
$env:TB_PATH = "D:\Chess\Syzygy"
```

### Download Sources

The built-in downloader uses these mirrors:
- **Primary:** https://tablebase.lichess.ovh/tables/standard/
- **Backup:** http://tablebase.sesse.net/syzygy/

For 6-piece tablebases (~150GB), consider downloading via torrent for better reliability.

---

## CMake Presets

CMakePresets.json provides IDE-agnostic build profiles.

### Windows Presets
- `debug` - MSVC Debug
- `release` - MSVC Release
- `relwithdebinfo` - MSVC RelWithDebInfo

### Linux/WSL Presets

**GCC Presets:**
- `wsl-debug` - GCC 13 Debug
- `wsl-release` - GCC 13 Release
- `wsl-relwithdebinfo` - GCC 13 RelWithDebInfo

**Clang Presets:**
- `wsl-clang-debug` - Clang 18 Debug
- `wsl-clang-release` - Clang 18 Release

### Using Presets Directly

```bash
# Configure
cmake --preset wsl-release

# Build
cmake --build cmake-build-wsl-release -j 30

# Test
ctest --test-dir cmake-build-wsl-release --output-on-failure
```

### CLion Integration

CLion automatically detects CMake presets:
1. Open CLion Settings → Build, Execution, Deployment → CMake
2. Presets appear in the configuration dropdown
3. Select desired preset (e.g., "WSL Clang Release")
4. Build and run normally

---

## CI/CD Pipeline

### GitHub Actions Workflow

**File:** `.github/workflows/ci-build.yml`

**Matrix:** 3 parallel jobs
```yaml
Jobs:
  - Windows Release (MSVC 2022) → artifacts
  - Linux GCC Release (GCC 13) → artifacts
  - Linux Clang Release (Clang 18) → validation
```

### Why Only Release Builds in CI?

- **Sufficient validation** - Release builds catch all compilation errors
- **50% faster CI** - 3 jobs instead of 6 (Debug + Release)
- **Debug available locally** - Developers build Debug when needed
- **Artifact focus** - Release artifacts for distribution

### CI Build Times

**First Run (no cache):**
- Windows: ~20-30 minutes (vcpkg + build)
- Linux GCC: ~15-25 minutes (vcpkg + build)
- Linux Clang: ~15-25 minutes (vcpkg + build)
- **Total parallel: ~25-30 minutes**

**Subsequent Runs (cached):**
- All platforms: ~5-10 minutes each
- **Total parallel: ~10 minutes**

### Artifacts

Available for download after successful CI run:
- `frankycpp-windows-Release.zip` - Windows MSVC binaries
- `frankycpp-linux-gcc-Release.tar.gz` - Linux GCC binaries

### Triggers

CI runs on:
- Push to `master` or `dev_*` branches
- Pull requests to `master`
- Manual trigger via `workflow_dispatch`

---

## Release Packaging

FrankyCPP uses CMake's `install()` command to create distributable release packages with proper directory structure and filtering.

### Overview

Running `cmake --install` creates:
- A versioned folder: `Release/FrankyCPP_v1.1/` containing:
  - Executable (`FrankyCPP_v1.1.exe` or `FrankyCPP_v1.1`)
  - `books/` folder (filtered - no cache files or test data)
  - `config/` folder (all YAML configuration files)
- A ZIP archive: `Release/FrankyCPP_v1.1.zip` ready for distribution

### Creating a Release

#### Option 1: CLion Install Menu (Recommended)

1. Open the **Build** menu
2. Click **Install**

This will:
- Create the versioned folder structure
- Filter out unwanted files
- Create the ZIP archive

#### Option 2: Command Line

**Windows:**
```powershell
cmake --install cmake-build-release --config Release
```

**Linux/WSL:**
```bash
cmake --install cmake-build-wsl-release
```

#### Option 3: Using Build Scripts

**Windows:**
```powershell
# Build and then package
.\build_windows.ps1 release
cmake --build cmake-build-release --target package_release
```

### Release Structure

After running `cmake --install`, you'll have:

```
Release/
├── FrankyCPP_v1.1/                    # Versioned folder (git ignored)
│   ├── FrankyCPP_v1.1.exe             # Main executable
│   ├── books/                          # Opening books
│   │   ├── book.txt                    # Default book
│   │   ├── 8moves_GM_LB.pgn           # GM games book
│   │   ├── ecoe.pgn                   # ECO book
│   │   ├── superbook.pgn              # Large opening book
│   │   ├── book_graham.txt            # Alternative books
│   │   ├── book_smalltest.txt
│   │   ├── pgn_test.pgn
│   │   └── ...
│   └── config/                         # Configuration files
│       ├── search.yaml                 # Search parameters
│       ├── eval.yaml                  # Evaluation parameters
│       └── FrankyCPP.cfg              # UCI options
└── FrankyCPP_v1.1.zip                 # ZIP archive (git tracked)
```

### What Gets Filtered Out

The packaging process automatically excludes:
- **Cache files**: `*.cache.*.bin` (platform-specific, regenerated on first run)
- **Large test files**: All `superbook*.pgn` files (large test-only files, not needed for distribution)
- **Build artifacts**: Temporary files from compilation

### Git Tracking

The `.gitignore` is configured to:
- ✅ **Track ZIP files**: `Release/FrankyCPP_v*.zip` (distribution artifacts)
- ❌ **Ignore folders**: `Release/FrankyCPP_v*/` (build artifacts)
- ❌ **Ignore flat install**: `Release/bin/` (development testing)

This means you can commit the ZIP files to version control for easy distribution, while keeping the source tree clean.

### Version Management

The package version is automatically derived from the CMake project version:

```cmake
project(FrankyCPP VERSION 1.1.0)
```

When you bump the version (e.g., to 1.2.0), the package target automatically creates:
- `Release/FrankyCPP_v1.2/`
- `Release/FrankyCPP_v1.2.zip`

No manual path updates needed!

### Platform-Specific Considerations

**Windows:**
- Executable has `.exe` extension
- ZIP created using CMake's built-in `tar` command with `--format=zip`
- Works with any Windows build directory (cmake-build-release, cmake-build-win-release, etc.)

**Linux/WSL:**
- Executable has no extension
- Same ZIP mechanism (cross-platform compatible)
- Works with Linux build directories (cmake-build-wsl-release, etc.)

### Testing the Release

After creating a package:

1. **Extract the ZIP** to a test location
2. **Navigate** to the extracted folder
3. **Run the executable**:
   ```powershell
   # Windows
   .\FrankyCPP_v1.1.exe
   
   # Linux
   ./FrankyCPP_v1.1
   ```
4. **Verify** that the opening book loads (cache will be created on first run)
5. **Test UCI commands**:
   ```
   uci
   isready
   position startpos
   go depth 10
   quit
   ```

### Advanced: Customizing the Package

The packaging logic is defined in:
- **Main target**: `CMakeLists.txt` (root) - `package_release` target
- **Book filtering**: `cmake/CopyBooksFiltered.cmake` - filters files during copy

To customize what gets included:

1. Edit `cmake/CopyBooksFiltered.cmake` to modify exclusion patterns
2. Edit the `install(CODE ...)` section in `src/CMakeLists.txt` to add new files/folders

Example: Adding a README to releases:

```cmake
# In src/CMakeLists.txt, inside install(CODE ...) block
# Add after the config files copy:
file(INSTALL \"\${PROJECT_SOURCE_DIR}/README.md\"
     DESTINATION \"\${RELEASE_DIR}\"
     RENAME \"README.txt\")
```

---

## Troubleshooting

### Windows Issues

#### MSVC Not Found
```powershell
# Install Visual Studio 2022 with C++ workload
# Or run from "x64 Native Tools Command Prompt for VS 2022"
```

#### vcpkg Bootstrap Fails
```powershell
# Manual vcpkg setup
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = "C:\vcpkg"
```

#### Ninja Not Found
```powershell
choco install ninja -y
```

### Linux Issues

#### Clang 18 Not Found
```bash
# Install from LLVM repository
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
sudo add-apt-repository "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-18 main"
sudo apt update
sudo apt install clang-18
```

#### GCC 13 Not Found
```bash
# Add Ubuntu Toolchain PPA
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-13 g++-13
```

#### vcpkg Build Slow
```bash
# Enable parallel vcpkg builds (done automatically by build script)
export VCPKG_MAX_CONCURRENCY=$(nproc)
```

#### std::format Not Found
```bash
# Ensure GCC 13+ is installed (required for std::format)
gcc-13 --version  # Must be 13.0 or higher

# For Clang, ensure libstdc++ from GCC 13 is available
sudo apt install g++-13
```

### Build Issues

#### Clean Rebuild
```bash
# Remove build directories
rm -rf cmake-build-*

# Rebuild from scratch
./build_wsl.sh release gcc
```

#### vcpkg Cache Issues
```bash
# Clean vcpkg cache
rm -rf $VCPKG_ROOT/buildtrees/*
rm -rf $VCPKG_ROOT/packages/*

# Reconfigure
cmake --preset wsl-release
```

---

## Technical Details

### C++20 Features Used

**Critical Dependencies:**
- `std::format` - Extensive use (20+ locations)
- `constexpr` enhancements - Compile-time Move construction
- Designated initializers - Structure initialization

**Compiler Requirements:**

| Feature     | GCC 13   | Clang 18        | MSVC 2022 |
|-------------|----------|-----------------|-----------|
| std::format | ✅ Native | ✅ Via libstdc++ | ✅ Native  |
| constexpr   | ✅        | ✅               | ✅         |
| Ranges      | ✅        | ✅               | ✅         |
| Concepts    | ✅        | ✅               | ✅         |

**Reference:** See `docs/CPP20_Feature_Support.md` for comprehensive feature matrix.

### Build System Architecture

**CMake + vcpkg:**
- Manifest mode (`vcpkg.json`) for reproducible builds
- Multi-config generators for Windows
- Single-config generators for Linux
- Precompiled headers for faster builds

**Optimization:**
- GCC: LTO enabled for Release builds
- Clang: LTO disabled (linking issues with static libraries)
- MSVC: LTO enabled for Release builds

### Position.cpp Inline Fix

**Issue:** Clang failed to link private helper functions marked `inline` in .cpp file.

**Solution:** Removed `inline` keyword from:
- `Position::movePiece()`
- `Position::putPiece()`
- `Position::removePiece()`

**Reason:** Clang strictly follows C++ rules - inline functions only used within translation unit don't generate external symbols, breaking friend test access.

**Performance Impact:** None - compilers still optimize with `-O3`.

### Standard Library Strategy

**Linux:** All compilers use libstdc++ from GCC 13
- GCC: Uses its own libstdc++ (native)
- Clang: Configured to use GCC's libstdc++ (compatibility)
- vcpkg: Builds libraries with libstdc++

**Benefit:** Consistent behavior, no linking issues, full std::format support.

### Build Performance

| Configuration | First Build | Incremental | PCH |
|---------------|-------------|-------------|-----|
| Windows MSVC  | ~10-15 min  | ~1-2 min    | ✅   |
| Linux GCC     | ~5-10 min   | ~1-2 min    | ✅   |
| Linux Clang   | ~5-10 min   | ~1-2 min    | ✅   |

**Note:** First build includes vcpkg dependency compilation (Boost, etc.)

### Production Builds (Compile-Time Feature Stripping)

New in v1.4, **Production Builds** enable a special compile mode:
- **Flag:** `FRANKYCPP_PRODUCTION=ON`
- **Effect:** Strips non-essential configuration options and statistics at compile time.
- **Mechanism:** Transforms mutable config members into `static constexpr` constants, enabling the compiler to eliminate entire branches of unused code (dead code elimination).
- **Benefit:** ~2-5% performance improvement (NPS) and smaller binary size.
- **Trade-off:** Runtime configuration (UCI) is limited to essential settings only.

To build:
- **Windows:** `cmake --preset win-release-production`
- **Linux:** `cmake --preset wsl-release-production`

See `docs/Compile_Time_Stripping.md` for developer details.

---

## Additional Documentation

- **Architecture.md** - System architecture and design decisions
- **FrankyCPP_Codebase_Review.md** - Comprehensive codebase analysis
- **CPP20_Feature_Support.md** - Detailed C++20 feature matrix
- **Logger.md** - Logging system documentation
- **CLion_WSL_Setup.md** - CLion with WSL configuration
- **engine-interface.txt** - UCI protocol reference

---

## Summary

### All Platforms Working ✅

| Platform  | Compiler  | Status | Tests    | Build Time |
|-----------|-----------|--------|----------|------------|
| Windows   | MSVC 2022 | ✅      | 266/266  | ~10-15 min |
| Linux/WSL | GCC 13    | ✅      | 266/266  | ~5-10 min  |
| Linux/WSL | Clang 18  | ✅      | All pass | ~5-10 min  |

### Quick Commands Reference

```bash
# Setup (one-time)
./setup_linux_build_env.sh --install  # Linux
.\setup_windows_build_env.ps1         # Windows

# Build
./build_wsl.sh release gcc            # Linux GCC
./build_wsl.sh release clang          # Linux Clang
.\build_windows.ps1 release           # Windows

# Test
./cmake-build-wsl-release/test/FrankyCPP_v0.7_Test
.\cmake-build-win-release\test\FrankyCPP_v0.7_Test.exe
```

**Ready to build!** 🚀
