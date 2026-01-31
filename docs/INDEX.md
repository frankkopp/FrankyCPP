# FrankyCPP Documentation Index

Quick reference to all project documentation.

---

## Getting Started


### 🚀 [BUILD_GUIDE.md](BUILD_GUIDE.md)
**Comprehensive build instructions for all platforms**
- Windows (MSVC 2022)
- Linux/WSL with GCC 13
- Linux/WSL with Clang 18
- CMake presets and CLion integration
- CI/CD pipeline overview
- Troubleshooting guide

**Start here if you want to build FrankyCPP!**

---

## Architecture & Design

### 📐 [Architecture.md](Architecture.md)
**System architecture and design decisions**
- Module structure
- Core abstractions (Position, Move, Bitboard)
- Search algorithm design
- Evaluation approach
- Threading model

### 📋 [FrankyCPP_Codebase_Review.md](FrankyCPP_Codebase_Review.md)
**Comprehensive codebase analysis**
- Executive summary and project status
- Build system overview
- Code quality assessment
- Testing infrastructure
- Roadmap and future improvements
- Accomplishment tracking

---

## Technical References

### 🔧 [CPP20_Feature_Support.md](CPP20_Feature_Support.md)
**C++20 feature support across compilers**
- Feature usage in FrankyCPP (std::format, constexpr, etc.)
- Compiler version requirements
- GCC 13 vs Clang 18 vs MSVC 2022
- std::format implementation details
- Build system validation strategy

### 📝 [Logger.md](Logger.md)
**Logging system documentation**
- spdlog integration
- Log levels and categories
- Configuration options
- Performance considerations

### 🔌 [engine-interface.txt](engine-interface.md)
**UCI protocol reference**
- Command reference
- Option descriptions
- Communication protocol

---

## IDE & Development

### 💻 [CLion_WSL_Setup.md](CLion_WSL_Setup.md)
**CLion with WSL configuration**
- WSL toolchain setup
- CMake profile configuration
- Remote debugging setup
- vcpkg integration

---

## Quick Reference

### Build Commands

**Windows:**
```powershell
.\setup_windows_build_env.ps1  # One-time setup
.\build_windows.ps1 release    # Build
```

**Linux/WSL:**
```bash
./setup_linux_build_env.sh --install  # One-time setup
./build_wsl.sh release gcc            # Build with GCC
./build_wsl.sh release clang          # Build with Clang
```

### Documentation by Topic

| Topic | Document |
|-------|----------|
| **First-time setup** | BUILD_GUIDE.md |
| **Architecture overview** | Architecture.md |
| **Project status** | FrankyCPP_Codebase_Review.md |
| **C++20 features** | CPP20_Feature_Support.md |
| **Logging** | Logger.md |
| **UCI protocol** | engine-interface.txt |
| **CLion IDE** | CLion_WSL_Setup.md |

### Key Files in Root Directory

- **README.md** - Project overview and quick start
- **CMakeLists.txt** - Main build configuration
- **CMakePresets.json** - CMake build profiles
- **vcpkg.json** - Dependency manifest
- **build_windows.ps1** - Windows build script
- **build_wsl.sh** - Linux/WSL build script
- **setup_windows_build_env.ps1** - Windows environment setup
- **setup_linux_build_env.sh** - Linux/WSL environment setup

---

## Documentation Status ✅

All documentation is current as of **2026-01-31**:
- ✅ Build instructions for all platforms
- ✅ Architecture and design documentation
- ✅ Technical references up to date
- ✅ IDE setup guides available
- ✅ Obsolete files removed

---

**Last updated:** 2026-01-31
