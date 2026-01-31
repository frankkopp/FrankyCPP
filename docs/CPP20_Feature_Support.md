# C++20 Feature Support - FrankyCPP

**Date:** 2026-01-31  
**Project:** FrankyCPP v0.7

---

## Executive Summary

FrankyCPP requires **C++20** with full support for `std::format`, which has specific compiler version requirements:

| Compiler | Minimum Version | Recommended | Notes |
|----------|----------------|-------------|-------|
| **GCC** | 13.0+ | 13.0+ | ✅ Full `std::format` support in libstdc++ |
| **Clang** | 15.0+ | 18.0+ | ✅ Clang 18+ has `std::format` in libc++; Clang 15-17 use libstdc++ |
| **MSVC** | 2019 16.10+ (v1929+) | 2022 | ✅ Full `std::format` support |

---

## C++20 Features Used in FrankyCPP

### 1. `std::format` (Critical)

**Usage:** Extensive throughout the codebase
- Type formatting (Move, Bitboard, Square, etc.)
- Logging and debug output
- Opening book display
- Performance metrics

**Files with `std::format`:**
- `src/types/move.h` - Move formatting
- `src/types/timeunits.h` - Time formatting
- `src/types/macros.h` - Formatter macros (ENABLE_FORMATTER_AS_STRING_VIEW_ON, etc.)
- `src/openingbook/OpeningBook.h` - Cache file naming
- `src/openingbook/OpeningBook.cpp` - Tree display
- `test/types/TypesTest.cpp` - Test output

**Why Critical:** `std::format` is used in 20+ locations. Cannot compile without it.

### 2. `constexpr` Enhancements

**Usage:** Compile-time evaluation for:
- Move construction and manipulation
- Attack table generation
- Magic number initialization
- Value type operations

**Examples:**
```cpp
constexpr Move move = Move::normal(SQ_A1, SQ_H1);
constexpr auto move1 = Move::enPassant(SQ_F4, SQ_E3);
```

**Compiler Support:** All C++20 compilers (GCC 10+, Clang 10+, MSVC 2019+)

### 3. Designated Initializers

**Usage:** Structure initialization

**Compiler Support:** All C++20 compilers

### 4. Three-Way Comparison (`<=>`)

**Usage:** Not currently used (could be added for cleaner comparisons)

### 5. Ranges

**Usage:** Not currently used (could leverage for move generation)

### 6. Concepts

**Usage:** Minimal (comment reference only)
- Potential future use for type constraints

---

## Compiler-Specific Details

### GCC 13+

**Status:** ✅ Full Support (Primary Compiler)

**C++20 Features:**
- ✅ `std::format` - Complete implementation
- ✅ `constexpr` enhancements - Full support
- ✅ Designated initializers - Yes
- ✅ Ranges - Complete
- ✅ Concepts - Complete
- ✅ Coroutines - Yes
- ✅ Modules - Experimental

**Library:** libstdc++ (GCC's standard library)

**Configuration:**
```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

---

### Clang 18+ (Recommended)

**Status:** ✅ Full Support (Recommended for Cross-Compiler Testing)

**C++20 Features:**
- ✅ `std::format` - Available in libc++ (Clang's native library)
- ✅ `constexpr` enhancements - Full support
- ✅ Designated initializers - Yes
- ✅ Ranges - Complete
- ✅ Concepts - Full support
- ✅ Coroutines - Yes
- ⚠️ Modules - Experimental

**Library:** libc++ (Clang's standard library with full C++20 support)

**Configuration:**
```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Clang 18+ uses libc++ by default (no special configuration needed)
```

**CMake Presets:**
```json
{
  "name": "wsl-clang-base",
  "cacheVariables": {
    "CMAKE_C_COMPILER": "clang-18",
    "CMAKE_CXX_COMPILER": "clang++-18"
  }
}
```

---

### Clang 15-17 (with libstdc++)

**Status:** ⚠️ Supported via GCC's libstdc++ (Fallback)

**Important:** Clang 15-17's own libc++ does NOT have complete `std::format`. On Linux, we configure Clang to use GCC's libstdc++ as a fallback.

**C++20 Features:**
- ⚠️ `std::format` - Via libstdc++ from GCC 13+ (NOT from Clang's libc++)
- ✅ `constexpr` enhancements - Full support
- ✅ Designated initializers - Yes
- ✅ Ranges - Via libstdc++
- ✅ Concepts - Full support
- ✅ Coroutines - Yes
- ⚠️ Modules - Experimental

**Library:** libstdc++ (borrowed from GCC 13+)

**Configuration:**
```cmake
# In CMakeLists.txt (automatic for Clang < 18 on Linux)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "18.0")
    add_compile_options(-stdlib=libstdc++)
    add_link_options(-stdlib=libstdc++)
endif()
```


**Verification:**
```bash
# Check what standard library Clang uses
echo '#include <version>' | clang++-15 -E -x c++ - | grep -i "GLIBCXX"
# If found, it's using libstdc++

# Check std::format availability
echo '#include <format>' | clang++-15 -std=c++20 -stdlib=libstdc++ -fsyntax-only -x c++ -
# Should succeed if GCC 13+ is installed
```

---

### MSVC 2019 16.10+ / 2022

**Status:** ✅ Full Support (Windows Primary)

**C++20 Features:**
- ✅ `std::format` - Complete implementation (since 16.10)
- ✅ `constexpr` enhancements - Full support
- ✅ Designated initializers - Yes
- ✅ Ranges - Complete
- ✅ Concepts - Complete
- ✅ Coroutines - Yes
- ⚠️ Modules - Experimental

**Library:** Microsoft STL

**Configuration:**
```cmake
set(CMAKE_CXX_STANDARD 20)
# MSVC automatically uses C++20 features when standard is set
```

---

## `std::format` Implementation Status

### Why `std::format` is Problematic

`std::format` was added in C++20 but implementation varies significantly:

| Compiler | Version | `std::format` Status |
|----------|---------|---------------------|
| **GCC** | 11 | ❌ Not available |
| **GCC** | 12 | ❌ Not available |
| **GCC** | 13 | ✅ **Full support** |
| **GCC** | 14 | ✅ Full support |
| **Clang (libc++)** | 15 | ❌ Not available |
| **Clang (libc++)** | 16 | 🟡 Experimental |
| **Clang (libc++)** | 17 | 🟡 Partial |
| **Clang (libc++)** | 18 | ✅ **Full support** |
| **Clang (libstdc++)** | 15+ | ✅ **Via GCC 13+ libstdc++** |
| **MSVC** | 2019 16.9 | ❌ Not available |
| **MSVC** | 2019 16.10+ | ✅ **Full support** |
| **MSVC** | 2022 | ✅ Full support |

### Our Solution

**Recommended: Clang 18**
- Has native `std::format` support in libc++
- No workarounds needed
- Clean, straightforward configuration

**Fallback: Clang 15-17 with libstdc++**
- Uses GCC 13's libstdc++ for `std::format`
- Requires GCC 13+ to be installed
- CMake automatically configures `-stdlib=libstdc++`

**Windows:** MSVC 2022 (has native `std::format`)

**Linux:** 
- Primary: GCC 13 (has native `std::format`)
- Cross-compiler: Clang 18 (has native `std::format` in libc++)
- Fallback: Clang 15-17 with `-stdlib=libstdc++` (uses GCC 13's `std::format`)

This approach ensures:
- ✅ Consistent `std::format` behavior
- ✅ Simple configuration with Clang 18
- ✅ CI tests validate both GCC and Clang compilation
- ✅ Future-proof (Clang 18 is the present/future)

---

## Build System Validation

### CMakeLists.txt Checks

```cmake
# GCC version check
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13.0")
        message(FATAL_ERROR "GCC 13.0+ required for std::format")
    endif()
endif()

# Clang version check + libstdc++ configuration
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15.0")
        message(FATAL_ERROR "Clang 15.0+ required")
    endif()
    
    # On Linux, use libstdc++ from GCC 13+
    if(UNIX AND NOT APPLE)
        add_compile_options(-stdlib=libstdc++)
        add_link_options(-stdlib=libstdc++)
        # Verify GCC 13+ is available
        find_program(GXX_PATH g++-13 g++)
        # ... validation logic ...
    endif()
endif()

# MSVC version check
elseif(MSVC)
    if(MSVC_VERSION LESS 1929)  # 16.10
        message(FATAL_ERROR "MSVC 2019 16.10+ required for std::format")
    endif()
endif()
```

### Setup Scripts

**Linux Setup (`setup_linux_build_env.sh`):**
```bash
# Installs GCC 13 (required for libstdc++ with std::format)
sudo apt-add-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-13 g++-13

# Installs Clang 15 (will use GCC 13's libstdc++)
sudo apt install clang-15

# Sets GCC 13 as default
sudo update-alternatives --set gcc /usr/bin/gcc-13
```

**Windows Setup (`setup_windows_build_env.ps1`):**
- Validates Visual Studio 2019 16.10+ or Visual Studio 2022
- MSVC has native `std::format` support

---

## Testing Strategy

### Local Testing

```bash
# Test with GCC 13 (native std::format)
./build_wsl.sh release gcc

# Test with Clang 15 (std::format via libstdc++)
./build_wsl.sh release clang

# Both should compile successfully
```

### CI Testing

GitHub Actions matrix:
```yaml
matrix:
  compiler: [gcc, clang]
  config: [Debug, Release]
```

**Results in 4 Linux jobs:**
1. GCC Debug - Uses GCC 13's libstdc++
2. GCC Release - Uses GCC 13's libstdc++
3. Clang Debug - Uses GCC 13's libstdc++ (via `-stdlib=libstdc++`)
4. Clang Release - Uses GCC 13's libstdc++ (via `-stdlib=libstdc++`)

All 4 jobs use the same standard library implementation, ensuring consistency.

---

## Future Considerations

### When Clang's libc++ Gets `std::format`

**Clang 18+** has more complete `std::format` in libc++. Once stable:

**Option 1:** Keep using libstdc++ (safest, most tested)
```cmake
# Keep current configuration
add_compile_options(-stdlib=libstdc++)
```

**Option 2:** Switch to libc++ (Clang's native library)
```cmake
# Allow Clang to use its default libc++
# Remove: add_compile_options(-stdlib=libstdc++)
```

**Recommendation:** Stick with libstdc++ until Clang 18+ is widely available and libc++'s `std::format` is proven stable.

### macOS Support

**macOS uses Clang + libc++** by default:
- macOS Clang is based on upstream Clang
- As of macOS Sonoma (14), Xcode 15 uses Clang 15+
- **Issue:** libc++ on macOS may not have `std::format` yet

**Solutions for macOS:**
1. **Wait for macOS with Clang 17+** (has better `std::format`)
2. **Use Homebrew GCC 13** and libstdc++
3. **Use fmt library** as fallback (alternative to `std::format`)

---

## Alternative: fmt Library

If compiler support becomes an issue, consider using the [fmt library](https://github.com/fmtlib/fmt):

**Advantages:**
- `std::format` is based on fmt
- Available on all platforms/compilers
- Can be swapped with `std::format` later
- vcpkg package available

**Migration:**
```cpp
// Current:
#include <format>
std::format("...", args);

// Alternative:
#include <fmt/format.h>
fmt::format("...", args);
```

**Not currently needed** - our libstdc++ approach works well.

---

## Summary

### Current Status ✅

| Platform | Compiler | `std::format` Source | Status |
|----------|----------|---------------------|--------|
| Windows | MSVC 2022 | Microsoft STL | ✅ Native |
| Linux | GCC 13 | libstdc++ | ✅ Native |
| Linux | Clang 15 | libstdc++ (GCC 13) | ✅ Via `-stdlib=libstdc++` |
| macOS | Clang | libc++ | ⚠️ Not yet tested (may need GCC/Homebrew) |

### Key Takeaways

1. ✅ **GCC 13 is required** - Provides libstdc++ with `std::format`
2. ✅ **Clang 15 works** - When configured to use GCC 13's libstdc++
3. ✅ **MSVC 2022 works** - Has native `std::format` support
4. ✅ **CMakeLists.txt handles it** - Automatically configures Clang to use libstdc++
5. ✅ **CI validates both** - GCC and Clang builds tested
6. ⚠️ **macOS TBD** - May need special handling

### Commands to Verify

```bash
# Check compiler versions
gcc-13 --version      # Must be 13.x+
clang-15 --version    # Must be 15.x+

# Test std::format with Clang
echo '#include <format>
int main() { auto s = std::format("test"); }' | \
  clang++-15 -std=c++20 -stdlib=libstdc++ -x c++ -

# Should succeed if GCC 13+ is installed

# Build with both compilers
./build_wsl.sh release gcc     # GCC 13
./build_wsl.sh release clang   # Clang 15 + libstdc++
```

Both builds should succeed! 🎯
