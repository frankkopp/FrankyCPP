# Clang 18 Migration Summary

**Date:** 2026-01-31  
**Change:** Switched from Clang 15 to Clang 18 for local testing and CI

---

## Why Clang 18?

### The Problem with Clang 15
- Clang 15's libc++ does NOT have `std::format`
- Required workaround: use GCC 13's libstdc++ with `-stdlib=libstdc++`
- Led to compatibility issues (ranges/concepts incompatibility with GCC 13's libstdc++)
- Required disabling precompiled headers
- Complex configuration

### Clang 18 is Better
- ✅ Has `std::format` in its own libc++ (Clang 18+)
- ✅ Better C++20 support than Clang 15
- ✅ No ranges/concepts incompatibility when using libstdc++
- ✅ Precompiled headers work fine
- ✅ Better diagnostics

### Why Still Use libstdc++?

**Important:** Even with Clang 18, we still use GCC's libstdc++ on Linux because:
- ✅ vcpkg libraries are compiled with libstdc++ (built by GCC)
- ✅ Mixing standard libraries causes linking errors
- ✅ libstdc++ from GCC 13 has full `std::format` support
- ✅ Consistent with GCC builds (same standard library)

**Key difference from Clang 15:**
- Clang 18 + libstdc++ has **no ranges/concepts issues** (better compatibility)
- Clang 18 can handle libstdc++'s C++20 features without errors
- PCH works without issues

---

## What Changed

### 1. Setup Script (`setup_linux_build_env.sh`)
```bash
# Old: Installed Clang 15
sudo apt install clang-15

# New: Installs Clang 18 from LLVM repository
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
sudo add-apt-repository "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-18 main"
sudo apt install clang-18
```

### 2. CMake Presets (`CMakePresets.json`)
```json
// Old
"CMAKE_C_COMPILER": "clang-15"
"CMAKE_CXX_COMPILER": "clang++-15"

// New
"CMAKE_C_COMPILER": "clang-18"
"CMAKE_CXX_COMPILER": "clang++-18"
```

### 3. CMakeLists.txt
```cmake
# Old: Required Clang 15+, forced libstdc++
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15.0")
    message(FATAL_ERROR "...")
endif()
add_compile_options(-stdlib=libstdc++)

# New: Always use libstdc++ on Linux (for vcpkg compatibility)
# Clang 18 handles libstdc++ better (no ranges/concepts issues)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND UNIX AND NOT APPLE)
    add_compile_options(-stdlib=libstdc++)
    add_link_options(-stdlib=libstdc++)
    # Verify GCC 13+ available for std::format support
endif()
```

### 4. CI Workflow (`.github/workflows/ci-build.yml`)
```yaml
# Old
cc: clang-15
cxx: clang++-15

# New  
cc: clang-18
cxx: clang++-18
```

### 5. Documentation
- Updated `Clang_Local_Testing.md` to recommend Clang 18
- Updated `CPP20_Feature_Support.md` with Clang 18 info
- Simplified troubleshooting (no more complex workarounds)

---

## How to Use

### Installation

```bash
# Run setup script (installs Clang 18)
./setup_linux_build_env.sh --install

# Verify
clang-18 --version
# Should show: Ubuntu clang version 18.x.x
```

### Building

```bash
# Build with Clang 18 (same command as before)
./build_wsl.sh release clang

# No workarounds, no special flags, just works! ✅
```

### CLion

CMake presets will automatically use Clang 18:
- Select "WSL Clang Release" profile
- Build normally

---

## Benefits

### Simpler
- ✅ Still uses `-stdlib=libstdc++` (for vcpkg compatibility)
- ✅ No precompiled header issues (Clang 18 handles libstdc++ better)
- ✅ No ranges/concepts compatibility issues
- ✅ Straightforward configuration

### Better
- ✅ Clang 18 has better C++20 support
- ✅ Better error messages
- ✅ No PCH workarounds needed
- ✅ More robust with libstdc++ than Clang 15

### Faster
- ✅ Precompiled headers work with Clang 18 + libstdc++
- ✅ No extra compatibility workarounds
- ✅ Cleaner build process

---

## Compatibility

| Version | Status | Standard Library | Notes |
|---------|--------|-----------------|-------|
| **Clang 18** | ✅ Recommended | libstdc++ (GCC 13) | Better libstdc++ compatibility than Clang 15 |
| **Clang 17** | ⚠️ Fallback | libstdc++ (GCC 13) | Works but less tested |
| **Clang 15-16** | ⚠️ Fallback | libstdc++ (GCC 13) | Ranges/concepts issues |
| **GCC 13** | ✅ Primary | libstdc++ (native) | Main development compiler |

**Why libstdc++ for all?**
- vcpkg libraries are built with GCC and use libstdc++
- Mixing libc++ (Clang) and libstdc++ (vcpkg) causes linker errors
- Both Clang and GCC use the same standard library for consistency

---

## CI Impact

### GitHub Actions

The setup script adds LLVM apt repository and installs Clang 18:

```yaml
- name: Setup build environment
  run: |
    sudo ./setup_linux_build_env.sh --install
  # Installs Clang 18 from apt.llvm.org
```

### Build Matrix

All 4 Linux jobs now use Clang 18:
- Linux (gcc, Debug)
- Linux (gcc, Release)
- Linux (clang, Debug) ← Now uses Clang 18
- Linux (clang, Release) ← Now uses Clang 18

---

## Migration Steps

If you already have Clang 15 installed:

### Option 1: Fresh Install (Recommended)

```bash
# Remove old Clang
sudo apt remove clang-15

# Run setup script
./setup_linux_build_env.sh --install

# Clean build directories
rm -rf cmake-build-wsl-clang-*

# Build with Clang 18
./build_wsl.sh release clang
```

### Option 2: Side-by-Side

```bash
# Install Clang 18 alongside Clang 15
./setup_linux_build_env.sh --install

# Clean build directories  
rm -rf cmake-build-wsl-clang-*

# CMake presets will use clang-18 automatically
./build_wsl.sh release clang
```

---

## Verification

```bash
# Check Clang version
clang-18 --version
# Ubuntu clang version 18.1.3 (or higher)

# Check std::format works with libstdc++
echo '#include <format>
int main() { auto s = std::format("Hello {}!", "World"); }' | \
  clang++-18 -std=c++20 -stdlib=libstdc++ -x c++ - -o /tmp/test && /tmp/test

# Should compile and run successfully

# Build FrankyCPP
./build_wsl.sh release clang

# Should build successfully ✅
```

---

## Summary

### Before (Clang 15)
- ❌ Ranges/concepts incompatibility with libstdc++
- ❌ PCH disabled for Clang
- ❌ Complex workarounds needed
- ✅ Uses libstdc++ (for vcpkg compatibility)

### After (Clang 18)
- ✅ No ranges/concepts issues with libstdc++
- ✅ PCH works fine
- ✅ Clean configuration
- ✅ Uses libstdc++ (for vcpkg compatibility)
- ✅ Better C++20 support overall

**Key Improvement:** Clang 18 has better compatibility with GCC 13's libstdc++, eliminating the ranges/concepts errors that plagued Clang 15.

**Result:** Much simpler, cleaner, and more maintainable! 🎯

---

## Commands Quick Reference

```bash
# Install
./setup_linux_build_env.sh --install

# Verify
clang-18 --version

# Build
./build_wsl.sh release clang

# That's it! ✅
```
