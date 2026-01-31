# Local Clang Testing Guide

**Purpose:** Test FrankyCPP with Clang compiler locally to catch issues before CI runs.

**Recommended:** Clang 18 has full C++20 support including `std::format` in its own libc++. No workarounds needed!

---

## Quick Start

### 1. Setup (First Time Only)

Install Clang and all build dependencies:

```bash
# Run the setup script with --install flag
./setup_linux_build_env.sh --install

# This installs:
# - GCC 13 (primary compiler with libstdc++ std::format support)
# - Clang 18 (cross-compiler with full C++20 support)
# - All build tools (cmake, ninja, vcpkg, etc.)

# Verify both compilers are installed
gcc-13 --version    # Should be 13.x or higher
clang-18 --version  # Should be 18.x or higher
```

**Note on Clang Versions:**
- **Clang 18+**: ✅ Full C++20 support, `std::format` in libc++ (recommended)
- **Clang 15-17**: ⚠️ Requires GCC 13's libstdc++ for `std::format` (fallback supported)
- The setup script installs Clang 18 when available

### 2. Build with Clang

Use the enhanced `build_wsl.sh` script:

```bash
# Release build with Clang (recommended first test)
./build_wsl.sh release clang

# Debug build with Clang
./build_wsl.sh debug clang

# For comparison, build with GCC
./build_wsl.sh release gcc
./build_wsl.sh debug gcc
```

**Syntax:** `./build_wsl.sh [BUILD_MODE] [COMPILER]`
- **BUILD_MODE**: `debug`, `release`, `relwithdebinfo` (default: `release`)
- **COMPILER**: `gcc`, `clang` (default: `gcc`)

### 3. Using CMake Presets Directly

Alternative approach using CMake presets (for CLion or manual builds):

```bash
# Configure with Clang
cmake --preset wsl-clang-release

# Build
cmake --build cmake-build-wsl-clang-release --parallel

# Run tests
ctest --test-dir cmake-build-wsl-clang-release --output-on-failure
```

---

## Available Clang Presets

### Configure Presets
- `wsl-clang-debug` - Clang Debug build
- `wsl-clang-release` - Clang Release build

### Build Presets (auto-parallel)
- `wsl-clang-debug` - Uses 30 cores
- `wsl-clang-release` - Uses 30 cores

### Test Presets
- `wsl-clang-debug` - Run Debug tests
- `wsl-clang-release` - Run Release tests

---

## CLion Integration

### Method 1: Using CMake Presets (Recommended)

CLion automatically detects CMake presets from `CMakePresets.json`:

1. Open CLion Settings: `File` → `Settings` → `Build, Execution, Deployment` → `CMake`
2. Click **"Reload CMake Project"**
3. CLion will show all presets including:
   - `wsl-debug` (GCC Debug)
   - `wsl-release` (GCC Release)
   - `wsl-clang-debug` ✨ (Clang Debug)
   - `wsl-clang-release` ✨ (Clang Release)
4. Select preset from dropdown in top toolbar
5. Build and run as usual

### Method 2: Manual CMake Profile

If CLion doesn't auto-detect presets:

1. Go to: `File` → `Settings` → `Build, Execution, Deployment` → `CMake`
2. Click `+` to add new profile
3. Configure:
   - **Name:** `WSL Clang Release`
   - **Build type:** `Release`
   - **Toolchain:** `WSL Ubuntu (GCC 13)` (yes, use same toolchain)
   - **CMake options:**
     ```
     -DCMAKE_C_COMPILER=clang-15
     -DCMAKE_CXX_COMPILER=clang++-15
     -DCMAKE_TOOLCHAIN_FILE=$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
     -DVCPKG_TARGET_TRIPLET=x64-linux
     ```
   - **Build directory:** `cmake-build-wsl-clang-release`
   - **Build options:** `-j 30`

---

## Why Test with Clang Locally?

### 1. **Catch Compiler-Specific Issues Early**
- GCC and Clang have different warning sets
- Different template instantiation behavior
- Different optimization strategies
- Clang often has stricter standard compliance

### 2. **Match CI Environment**
- CI tests with both GCC 13 and Clang 15
- Local Clang test catches issues before push
- Faster feedback loop than waiting for CI

### 3. **Cross-Platform Preparation**
- Clang is the primary compiler for macOS
- Testing with Clang validates future macOS support
- Ensures portable C++20 code

### 4. **Better Error Messages**
- Clang often provides clearer error messages than GCC
- Helpful for debugging complex template errors
- Better diagnostics for C++20 features

---

## Troubleshooting

### Clang Not Found

```bash
# Install Clang 18 via setup script
./setup_linux_build_env.sh --install

# Or manually install
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
sudo add-apt-repository "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-18 main"
sudo apt update
sudo apt install clang-18
```

### Wrong Clang Version

The presets use `clang-18` explicitly. If you have a different version:

**Option 1:** Install Clang 18 (recommended)
```bash
sudo apt install clang-18
```

**Option 2:** Edit `CMakePresets.json` to use your version
```json
{
  "name": "wsl-clang-base",
  "cacheVariables": {
    "CMAKE_C_COMPILER": "clang-17",     // Change version
    "CMAKE_CXX_COMPILER": "clang++-17"  // Change version
  }
}
```

**Note:** Clang 15-17 require GCC 13+ for `std::format` support via libstdc++.

### std::format Not Found with Clang < 18

If using Clang 15-17, you need GCC 13's libstdc++:

```bash
# Install GCC 13
sudo apt install g++-13

# CMake will automatically configure Clang to use libstdc++
```

```bash
# Install Clang 15
sudo apt install clang-15

# Or run setup script
./setup_linux_build_env.sh --install
```

### Wrong Clang Version

The presets use `clang-15` explicitly. If you have a different version:

**Option 1:** Install Clang 15
```bash
sudo apt install clang-15
```

**Option 2:** Edit `CMakePresets.json` to use your version
```json
{
  "name": "wsl-clang-base",
  "cacheVariables": {
    "CMAKE_C_COMPILER": "clang-18",     // Change version
    "CMAKE_CXX_COMPILER": "clang++-18"  // Change version
  }
}
```

### vcpkg Issues with Clang

vcpkg packages are compiler-agnostic on Linux (triplet: `x64-linux`). They work with both GCC and Clang. If you encounter issues:

```bash
# Clean vcpkg cache
rm -rf $VCPKG_ROOT/buildtrees/*
rm -rf $VCPKG_ROOT/packages/*

# Reconfigure
cmake --preset wsl-clang-release
```

### Linking Errors

If you get linking errors about missing symbols:
- Ensure vcpkg packages were built properly
- Try cleaning and rebuilding: `rm -rf cmake-build-wsl-clang-*`
- Rebuild from scratch

---

## Comparison: GCC vs Clang

### Build the Same Code with Both

```bash
# Test with both compilers
./build_wsl.sh release gcc
./build_wsl.sh release clang

# Compare warnings
# (Clang often finds more potential issues)
```

### Performance Comparison

```bash
# Build optimized binaries
./build_wsl.sh release gcc
./build_wsl.sh release clang

# Run benchmarks
./cmake-build-wsl-release/testbench/FrankyCPP_v*_Bench
./cmake-build-wsl-clang-release/testbench/FrankyCPP_v*_Bench
```

---

## CI Integration

The CI workflow automatically tests with both compilers:

```yaml
matrix:
  compiler: [gcc, clang]
  config: [Debug, Release]
```

This creates 4 Linux build jobs:
- Linux GCC Debug
- Linux GCC Release (with artifacts)
- Linux Clang Debug
- Linux Clang Release

By testing locally with Clang, you ensure these CI jobs will pass.

---

## Best Practices

### Before Every Push

```bash
# Quick validation with both compilers
./build_wsl.sh release gcc    # Primary compiler
./build_wsl.sh release clang  # Cross-compiler validation
```

### When Adding New Features

```bash
# Test thoroughly with both compilers in debug mode
./build_wsl.sh debug gcc
./build_wsl.sh debug clang

# Fix any warnings/errors that appear
```

### Before Major Refactoring

```bash
# Ensure both compilers are happy
./build_wsl.sh release gcc
./build_wsl.sh release clang

# Run full test suite with both
./cmake-build-wsl-release/test/FrankyCPP_v*_Test
./cmake-build-wsl-clang-release/test/FrankyCPP_v*_Test
```

---

## Summary

✅ Setup script now installs Clang 15 automatically
✅ CMake presets configured for Clang builds
✅ `build_wsl.sh` supports compiler selection
✅ CLion integration via presets
✅ Matches CI test matrix (GCC + Clang)
✅ Catches cross-compiler issues early

**Quick command to remember:**
```bash
./build_wsl.sh release clang
```

This single command builds, tests, and validates your code with Clang! 🎯
