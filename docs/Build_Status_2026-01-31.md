# FrankyCPP Build Status - 2026-01-31

## Current State: All Local Builds Working ✅

### Supported Platforms

| Platform | Compiler | Version | Status | Notes |
|----------|----------|---------|--------|-------|
| **Windows** | MSVC | 2022 | ✅ Working | Primary development platform |
| **Linux/WSL** | GCC | 13.3.0 | ✅ Working | Primary compiler, native std::format |
| **Linux/WSL** | Clang | 18.1.3 | ✅ Working | Cross-compiler testing, uses libstdc++ |

---

## Build Configurations

### Windows (MSVC 2022)
```powershell
# Release build
.\build_windows.ps1 release

# Debug build  
.\build_windows.ps1 debug
```

**Status:** ✅ All 266 tests passing

### Linux/WSL (GCC 13)
```bash
# Release build (default)
./build_wsl.sh release gcc

# Debug build
./build_wsl.sh debug gcc
```

**Status:** ✅ All 266 tests passing

### Linux/WSL (Clang 18)
```bash
# Release build
./build_wsl.sh release clang

# Debug build
./build_wsl.sh debug clang
```

**Status:** ✅ All tests passing

---

## Key Changes Made

### 1. Clang 18 Support
- Upgraded from Clang 15 to Clang 18
- Uses libstdc++ from GCC 13 for vcpkg compatibility
- Fixed linking issues by removing inappropriate `inline` keywords from Position.cpp
- Disabled LTO for Clang to avoid static library linking issues

### 2. Setup Scripts
- `setup_windows_build_env.ps1` - Validates/installs Windows build environment
- `setup_linux_build_env.sh` - Installs GCC 13 and Clang 18 from LLVM repository

### 3. Build Scripts
- `build_windows.ps1` - Windows build with MSVC
- `build_wsl.sh` - Linux build with compiler selection (gcc/clang)

### 4. CMake Configuration
- Updated to require/recommend Clang 18+
- Automatically configures Clang to use libstdc++ on Linux
- Disabled LTO for Clang builds (avoids linking issues)
- All compilers use libstdc++ on Linux for consistency

### 5. Documentation
- `docs/Clang_Local_Testing.md` - Guide for local Clang testing
- `docs/CPP20_Feature_Support.md` - Comprehensive C++20 feature matrix
- `docs/Clang_18_Migration.md` - Migration notes from Clang 15 to 18

---

## Technical Details

### Why Clang 18 Uses libstdc++

Even though Clang 18 has `std::format` in its own libc++, we use GCC's libstdc++ because:
1. ✅ vcpkg libraries are built with GCC and use libstdc++
2. ✅ Mixing standard libraries causes linking errors
3. ✅ Consistent with GCC builds (same standard library)
4. ✅ libstdc++ from GCC 13 has full C++20 support

### Position.cpp Inline Fix

Removed `inline` keyword from private helper functions:
- `movePiece()`
- `putPiece()`  
- `removePiece()`

**Reason:** Clang strictly follows C++ rules for inline functions in .cpp files. When marked inline and only used within the translation unit, Clang doesn't generate external symbols, breaking friend test access. GCC was more lenient.

**Performance Impact:** None - compilers still optimize aggressively with `-O3` and can inline without the keyword.

### LTO Configuration

- **GCC:** LTO enabled (full optimization)
- **Clang:** LTO disabled (avoids linking issues with static libraries)
- **MSVC:** LTO enabled

---

## Build Performance

| Configuration | First Build | Incremental Build |
|--------------|-------------|-------------------|
| Windows MSVC | ~10-15 min | ~1-2 min |
| Linux GCC 13 | ~5-10 min | ~1-2 min |
| Linux Clang 18 | ~5-10 min | ~1-2 min |

**Note:** First build includes vcpkg dependency compilation (Boost, GoogleTest, etc.)

---

## CI Status

### GitHub Actions Workflow
- **File:** `.github/workflows/ci-build.yml`
- **Status:** ⚠️ Ready for deployment, needs testing

**Build Matrix:**
```yaml
Windows:
  - Debug (MSVC 2022)
  - Release (MSVC 2022) → artifacts

Linux:
  - GCC 13 Debug
  - GCC 13 Release → artifacts
  - Clang 18 Debug
  - Clang 18 Release
```

**Next Steps:**
1. Test CI workflow on GitHub
2. Fix any CI-specific issues
3. Validate artifacts

---

## Known Issues

### None for Local Builds ✅

All local build configurations are working correctly.

### Potential CI Issues (To Be Tested)

1. **Clang 18 availability** - GitHub runners may need LLVM repository setup
2. **vcpkg caching** - First CI run will be slow
3. **Environment variables** - CI detection working correctly?

---

## Files Modified

### Build System
- ✅ `CMakeLists.txt` - Clang 18 support, LTO configuration
- ✅ `CMakePresets.json` - Clang 18 presets
- ✅ `build_wsl.sh` - Compiler selection support
- ✅ `setup_linux_build_env.sh` - Clang 18 installation

### Source Code
- ✅ `src/chesscore/Position.cpp` - Removed inline keywords for friend test access

### CI/CD
- ✅ `.github/workflows/ci-build.yml` - Clang 18 matrix

### Documentation
- ✅ `README.md` - Updated with Clang support
- ✅ `docs/Clang_Local_Testing.md` - New guide
- ✅ `docs/CPP20_Feature_Support.md` - New reference
- ✅ `docs/Clang_18_Migration.md` - Migration notes
- ✅ `docs/FrankyCPP_Codebase_Review.md` - Updated status

---

## Testing Summary

### Local Tests
```bash
# Windows MSVC
.\build_windows.ps1 release
# Result: ✅ All 266 tests pass

# Linux GCC 13
./build_wsl.sh release gcc  
# Result: ✅ All 266 tests pass

# Linux Clang 18
./build_wsl.sh release clang
# Result: ✅ All tests pass
```

### Test Exclusions (CI)
Long-running tests excluded in CI:
- `*SpeedTests.*`
- `*TimingTests.*`

---

## Compiler Feature Matrix

| Feature | GCC 13 | Clang 18 | MSVC 2022 |
|---------|--------|----------|-----------|
| C++20 Standard | ✅ | ✅ | ✅ |
| std::format | ✅ Native | ✅ Via libstdc++ | ✅ Native |
| constexpr enhancements | ✅ | ✅ | ✅ |
| Ranges | ✅ | ✅ Via libstdc++ | ✅ |
| Concepts | ✅ | ✅ | ✅ |
| Modules | 🟡 Experimental | 🟡 Experimental | 🟡 Experimental |
| PCH Support | ✅ | ✅ | ✅ |
| LTO Support | ✅ Enabled | ❌ Disabled | ✅ Enabled |

---

## Quick Reference Commands

### Setup
```bash
# Linux - Install all build tools
./setup_linux_build_env.sh --install

# Windows - Validate environment
.\setup_windows_build_env.ps1
```

### Build
```bash
# GCC (primary)
./build_wsl.sh release gcc

# Clang (testing)
./build_wsl.sh release clang

# Windows
.\build_windows.ps1 release
```

### Verify Compilers
```bash
gcc-13 --version    # 13.3.0+
clang-18 --version  # 18.1.3+
```

---

## Next Phase: CI Deployment

1. ✅ **Phase 1:** Local builds working (COMPLETE)
2. ⏭️ **Phase 2:** Deploy and test GitHub Actions CI
3. ⏭️ **Phase 3:** Fix CI-specific issues
4. ⏭️ **Phase 4:** Enable CI for all branches

**Ready to proceed with CI testing!** 🚀
