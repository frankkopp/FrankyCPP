# FrankyCPP

Modern C++20 UCI Chess Engine

[![CI Build](https://github.com/frankkopp/FrankyCPP/actions/workflows/ci-build.yml/badge.svg?branch=dev_v0.7)](https://github.com/frankkopp/FrankyCPP/actions/workflows/ci-build.yml)
[![CodeQL](https://github.com/frankkopp/FrankyCPP/actions/workflows/codeql-analysis.yml/badge.svg?branch=dev_v0.7)](https://github.com/frankkopp/FrankyCPP/actions/workflows/codeql-analysis.yml)

## Version

**v0.7** - Production-ready release candidate
- ✅ Complete cross-platform support (Windows, Linux, macOS-ready)
- ✅ Modern C++20 codebase with full CI/CD
- ✅ Comprehensive test coverage (266+ tests)
- ✅ Professional build infrastructure
- ✅ YAML-based configuration framework
- ✅ Enhanced search and evaluation

**Previous versions:**
- v0.6 - Enhanced search, enhanced logging
- v0.5 - Enhanced eval and move to wrapper classes
- v0.4 - Simple eval
- v0.3 - Migration from FrankyGo

---

## Quick Start

```bash
# Windows
.\build_windows.ps1 release

# Linux/WSL
./build_wsl.sh release gcc
```

See **[docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md)** for complete instructions.

---

## Build

**📖 For comprehensive build instructions, see [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md)**

### Platform Support

| Platform | Compiler | Status | Tests |
|----------|----------|--------|-------|
| **Windows** | MSVC 2022 | ✅ Tested | 266/266 |
| **Linux/WSL** | GCC 13+ | ✅ Tested | 266/266 |
| **Linux/WSL** | Clang 18+ | ✅ Tested | 266/266 |
| **macOS** | Clang 18+ | 🔜 Ready | Not tested |

### Technology Stack

FrankyCPP uses CMake with target-scoped includes and third‑party libraries via vcpkg manifest mode.

- **C++ Standard:** C++20 (std::format, constexpr enhancements, etc.)
- **Build System:** CMake 3.22+ with Ninja generator
- **Package Manager:** vcpkg manifest mode
- **CI/CD:** GitHub Actions (Windows MSVC, Linux GCC/Clang)
- **Testing:** GoogleTest with 266+ unit tests
- **Benchmarking:** Google Benchmark for performance testing

### Pinned dependencies (via vcpkg manifest overrides)
- spdlog: v1.15.3 (header-only with C++20 std::format; no separate fmt dependency)
- googletest: v1.14.0
- google benchmark: v1.9.4

Boost components (program_options, serialization) are consumed via vcpkg as well (versions from the vcpkg baseline).

### CMake options
- `FRANKYCPP_BUILD_TESTS` (ON): Build unit tests (GoogleTest)
- `FRANKYCPP_BUILD_BENCHMARKS` (ON): Build benchmarks (Google Benchmark)
- `FRANKYCPP_USE_PCH` (ON): Enable precompiled headers for FrankyCPPlib
- `ENABLE_UNITY_BUILD` (OFF): Opt-in Unity/Jumbo builds for faster local compiles
- `STRICT_WARNINGS` (OFF): Treat warnings as errors (/WX) on MSVC
- `ENABLE_STD_EXECUTION` (ON): Define `HAS_EXECUTION_LIB` for code paths using `<execution>`
- `ENABLE_BMI2_PEXT` (ON): Define `HAS_PEXT`; on MSVC also adds `/arch:AVX2` to FrankyCPPlib
- `ENABLE_AVX2` (OFF): Adds `/arch:AVX2` to FrankyCPPlib explicitly
- `VCPKG_TARGET_TRIPLET` (x64-windows-static-md default on MSVC): Override vcpkg triplet if needed

Notes:
- Guard against dynamic vcpkg triplets on Windows: dynamic triplets (e.g., `x64-windows`) are rejected 
  to ensure a DLL-free executable; use a `-static` triplet (default is `x64-windows-static-md`).
- LTO/IPO is enabled for Release/RelWithDebInfo/MinSizeRel when supported by the toolchain.
- Library links (PUBLIC): `Boost::serialization`, `Boost::program_options`. Logging uses spdlog header-only 
  with std::format (no fmt linkage).
- **Build Environment Validation:** CMake configuration includes automatic checks for:
  - Compiler version and C++20 support
  - Required build tools (pkg-config, tar, unzip on Unix)
  - vcpkg installation and configuration
  - CPU architecture support

### Linux Build Environment Setup

For Linux/WSL, use the setup script:

```bash
# Validate environment (safe, no system modifications)
./setup_linux_build_env.sh

# Install dependencies (requires sudo, modifies system)
./setup_linux_build_env.sh --install

# Set VCPKG_ROOT in ~/.bashrc for existing vcpkg (e.g., CLion's vcpkg)
./setup_linux_build_env.sh --set-env

# Show help
./setup_linux_build_env.sh --help
```

**Default behavior is validation-only for safety.** Use `--install` or `--set-env` explicitly when you want to make changes.

This script:
- **Validates** (default): Checks all tools, versions, CPU features without making changes
- **Installs** (--install): Installs essential build tools, optional vcpkg tools, clones and bootstraps vcpkg, adds VCPKG_ROOT to ~/.bashrc
- **Sets Env Var** (--set-env): Adds VCPKG_ROOT to ~/.bashrc for detected vcpkg (e.g., CLion's installation)
- Detects CI/CD environments and adjusts behavior automatically
- Reports what's missing with installation instructions
- **Smart Detection**: Automatically detects CLion's vcpkg at `~/.vcpkg-clion/vcpkg`

### Windows Build Environment Setup

For Windows, use the setup script:

```powershell
# Validate environment (safe, no system modifications)
.\setup_windows_build_env.ps1

# Install vcpkg (if not already set up)
.\setup_windows_build_env.ps1 -Install

# Set VCPKG_ROOT for existing vcpkg (e.g., CLion's vcpkg)
.\setup_windows_build_env.ps1 -SetEnvVar

# Show help
.\setup_windows_build_env.ps1 -Help
```

**Default behavior is validation-only for safety.** Use `-Install` or `-SetEnvVar` explicitly when you want to make changes.

**Prerequisites (must be installed manually):**
- Visual Studio 2019 16.10+ or Visual Studio 2022 (with "Desktop development with C++" workload)
- CMake 3.16 or higher
- Ninja build system (recommended)
- Git

The script:
- **Validates** (default): Checks Visual Studio, CMake, vcpkg, build tools
- **Installs** (-Install): Automatically clones and bootstraps vcpkg to `C:\vcpkg`, sets VCPKG_ROOT
- **Sets Env Var** (-SetEnvVar): Sets VCPKG_ROOT for detected vcpkg (e.g., CLion's installation)
- Cannot install Visual Studio, CMake, or Ninja (these require manual installation)
- **Smart Detection**: Automatically detects CLion's vcpkg at `%USERPROFILE%\.vcpkg-clion\vcpkg`

**Note:** If you use CLion, the script will detect its managed vcpkg. Use `-SetEnvVar` to make it available for command-line builds, or use `-Install` to set up a separate system-wide vcpkg at `C:\vcpkg`.

---

## Quick Build Scripts

### Windows Build Script

Convenience script for building on Windows with CMake presets:

```powershell
# Release build (default)
.\build_windows.ps1

# Debug build
.\build_windows.ps1 debug

# Other configurations
.\build_windows.ps1 relwithdebinfo
.\build_windows.ps1 minsizerel

# Show help
.\build_windows.ps1 -Help
```

**Features:**
- Uses CMake presets (debug, release, relwithdebinfo, minsizerel)
- Validates VCPKG_ROOT and Visual Studio installation
- Automatically detects CLion's vcpkg if not set
- Builds with parallel compilation
- Runs tests (excluding slow tests)
- Reports build status and executable location

**First build:** 10-20 minutes (vcpkg dependencies compilation)  
**Subsequent builds:** 1-2 minutes (only project code)

### Linux/WSL Build Script

Convenience script for building on Linux/WSL:

```bash
# Release build with GCC (default)
./build_wsl.sh

# Debug build with GCC
./build_wsl.sh debug

# Release build with Clang (cross-compiler testing)
./build_wsl.sh release clang

# Debug build with Clang
./build_wsl.sh debug clang

# Show help
./build_wsl.sh --help
```

**Features:**
- Supports both GCC 13 and Clang 15 compilers
- Uses CMake presets (wsl-debug, wsl-release, wsl-clang-debug, wsl-clang-release)
- Validates VCPKG_ROOT environment variable
- Builds with parallel compilation (ninja)
- Runs tests (excluding slow tests)
- Reports build status and executable location

**Prerequisites:** Run `./setup_linux_build_env.sh --install` first to set up build environment.

---

### Manual Build (Windows, cmd.exe)
If building outside an IDE, initialize the MSVC environment first.

1) Open “x64 Native Tools Command Prompt for VS 2022” (or run vcvars64.bat)
2) Configure (RelWithDebInfo example):

```cmd
cmake -S D:\_DEV\FrankyCPP -B D:\_DEV\FrankyCPP\cmake-build-relwithdebinfo -G Ninja -DFRANKYCPP_BUILD_TESTS=ON -DFRANKYCPP_BUILD_BENCHMARKS=OFF
```

3) Build:

```cmd
cmake --build D:\_DEV\FrankyCPP\cmake-build-relwithdebinfo -j 8
```

4) Run tests (optional):

```cmd
ctest -C RelWithDebInfo --test-dir D:\_DEV\FrankyCPP\cmake-build-relwithdebinfo --output-on-failure
```

5) Install app, config, and books to `Release/bin` (optional):

```cmd
cmake --install D:\_DEV\FrankyCPP\cmake-build-relwithdebinfo --config RelWithDebInfo
```

CLion users can just reload CMake; the IDE sets up the MSVC environment automatically.

### Artifacts
- App: `FrankyCPP_v<major>.<minor>`
- Tests: `FrankyCPP_v<major>.<minor>_Test` (when `FRANKYCPP_BUILD_TESTS=ON`)
- Benchmarks: `FrankyCPP_v<major>.<minor>_Bench` (when `FRANKYCPP_BUILD_BENCHMARKS=ON`)
- Install (Release/RelWithDebInfo): copies `config/` and `books/` next to the binary in `Release/bin` (via `cmake --install`).

### Opening Book Cache Files

The opening book uses Boost serialization to cache compiled book data in `.bin` files for faster loading. These cache files are **platform-specific** and cannot be shared between Windows and Linux due to differences in native type sizes (e.g., `sizeof(long)`).

**Cache file naming:**
- Windows: `book.txt.cache.v0.7.win.bin`
- Linux: `book.txt.cache.v0.7.linux.bin`
- macOS: `book.txt.cache.v0.7.macos.bin`

**What happens:**
- First run: Opening book is parsed from `.txt` or `.pgn` files and a platform-specific `.cache.v<version>.<platform>.bin` file is created
- Subsequent runs: Platform-specific cache file is loaded for faster startup
- Cross-platform: Each platform has its own cache file, no conflicts or regeneration needed

**Benefits:**
- ✅ No cache regeneration when switching between Windows and Linux builds on same machine
- ✅ Both caches can coexist in the same directory
- ✅ Automatic platform detection

**Note:** Cache files (`*.cache.*.bin`) are excluded from version control via `.gitignore`.
