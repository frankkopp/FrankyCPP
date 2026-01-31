# FrankyCPP v1.0.0 - Production Release 🎉

FrankyCPP is now production-ready with complete cross-platform support and professional infrastructure.

## Highlights

- ✅ **Cross-platform:** Windows (MSVC 2022), Linux (GCC 13, Clang 18), macOS-ready
- ✅ **Modern C++20:** Full feature usage including std::format
- ✅ **Comprehensive testing:** 266+ unit tests, all passing on all platforms
- ✅ **CI/CD:** GitHub Actions validates every push across all platforms
- ✅ **Professional infrastructure:** Complete documentation and automated builds

## Features

- **UCI chess protocol** - Full implementation for GUI compatibility
- **Alpha-beta search** - Advanced pruning techniques and optimizations
- **Configurable evaluation** - YAML-based runtime configuration
- **Opening book support** - PGN format parsing and caching
- **Cross-compiler validated** - MSVC, GCC 13, and Clang 18

## Download

See **Assets** below for pre-built binaries:
- Windows (MSVC 2022) - `frankycpp-windows-Release.zip`
- Linux (GCC 13) - `frankycpp-linux-gcc-Release.tar.gz`

## Building from Source

See [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md) for complete instructions.

**Quick start:**
```bash
# Windows
.\build_windows.ps1 release

# Linux
./build_wsl.sh release gcc
```

## What's Next (v1.x Roadmap)

See [docs/V1.0_RELEASE_READINESS.md](docs/V1.0_RELEASE_READINESS.md) for details:
- Lazy SMP multi-threading
- NNUE evaluation
- Syzygy tablebase support
- Performance optimizations

## Full Changelog

**v0.7 → v1.0.0 accomplishments:**
- ✅ Professional build infrastructure with CMake and vcpkg
- ✅ GitHub Actions CI/CD deployment (Windows, Linux GCC, Linux Clang)
- ✅ Comprehensive documentation (8 guides covering all aspects)
- ✅ Complete cross-platform support (Windows/Linux working, macOS-ready)
- ✅ Clang 18 support with full C++20 compatibility
- ✅ YAML-based configuration system overhaul
- ✅ License compliance (MIT) - removed GPL references
- ✅ 266+ unit tests with full coverage
- ✅ Automated setup scripts for all platforms

**Technical improvements:**
- Modern C++20 codebase (std::format, constexpr, concepts)
- Precompiled headers for faster builds
- LTO/IPO for optimized Release builds
- Sanitizer support (ASan, UBSan) for Debug builds
- CMake presets for IDE integration

---

**Ready for production use!** 🚀

Play chess with FrankyCPP using any UCI-compatible chess GUI.
