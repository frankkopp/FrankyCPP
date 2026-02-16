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

## Engine Arena - Testing Framework

### 🏟️ [arena/README.md](arena/README.md) **(NEW in v1.1)**
**Quick start guide for Engine Arena**
- Overview and key features
- Quick start commands
- Common workflows (testing, comparison, matching)
- Troubleshooting
- Command-line reference

### 📝 [arena/Configuration.md](arena/Configuration.md)
**Complete configuration reference**
- YAML configuration structure
- Test suite configuration (EPD tests)
- Match configuration (cutechess-cli)
- Global settings and validation
- Configuration examples

### 📊 [arena/Results.md](arena/Results.md)
**Result file formats and analysis**
- Test suite JSON format
- Match result JSON format
- PGN file structure
- Comparison report format
- Analysis workflows and scripts
- Exporting results (CSV, database)

### 🛠️ [arena/Development.md](arena/Development.md)
**Developer guide for extending Arena**
- Architecture overview
- Class responsibilities
- Adding new features
- Testing guidelines
- Code style and conventions
- Future enhancement roadmap

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

## Specifications & Roadmaps

### 🎯 [V1_ENGINE_ENHANCEMENT_PLAN.md](specs/V1_ENGINE_ENHANCEMENT_PLAN.md)
**v1.x Engine Enhancement Roadmap**
- Phase-based implementation plan
- Search enhancements (singular extensions, check extensions, Lazy SMP)
- Evaluation improvements (NNUE neural networks)
- Endgame tablebases (Syzygy)
- Automated parameter tuning infrastructure
- Detailed specifications and success metrics

### 📊 [V1_ENGINE_STRENGTH_ROADMAP.md](specs/V1_ENGINE_STRENGTH_ROADMAP.md)
**Engine Strength Development Roadmap**
- Comprehensive v1.x to v2.0 development plan
- Tactical test suite baselines and targets
- Benchmark match configurations
- Risk assessment and mitigation
- References and resources

### ⚙️ [PLAN_Configuration_Refactor.md](specs/PLAN_Configuration_Refactor.md)
**Configuration System Refactoring Plan**
- Single source of truth for all config settings
- Auto-generation of str(), YAML, UCI options
- ConfigDef metadata system design
- Phased implementation approach
- Eliminates config duplication across 4-6 files

### 🎲 [PLAN_Syzygy_Tablebase_Support.md](specs/PLAN_Syzygy_Tablebase_Support.md)
**Syzygy Endgame Tablebase Integration**
- Fathom library integration
- WDL/DTZ probing at root and in-search
- Tablebase path discovery and validation
- Built-in download utility
- UCI options (SyzygyPath, SyzygyProbeDepth)

### ⚔️ [PLAN_QSearch_Quiet_Checks.md](specs/PLAN_QSearch_Quiet_Checks.md)
**Quiescence Search Quiet Check Enhancement**
- Adding non-capturing checks to qsearch
- Check evasion handling
- Performance considerations
- Implementation approach

### ⏱️ [PLAN_Speedtest_Benchmark.md](specs/PLAN_Speedtest_Benchmark.md)
**Performance Benchmark Infrastructure**
- Benchmark suite design
- Standardized test positions
- NPS and node count comparisons
- Regression detection

---

## Technical References

### 🔧 [CPP20_Feature_Support.md](CPP20_Feature_Support.md)
**C++20 feature support across compilers**
- Feature usage in FrankyCPP (std::format, constexpr, etc.)
- Compiler version requirements
- GCC 13 vs Clang 18 vs MSVC 2022
- std::format implementation details
- Build system validation strategy

### 🧵 [Lazy_SMP_Explained.md](Lazy_SMP_Explained.md)
**Lazy SMP Parallel Search**
- Lazy SMP algorithm explanation
- Thread coordination via shared TT
- Implementation considerations
- Performance characteristics

### 📝 [Logger.md](Logger.md)
**Logging system documentation**
- spdlog integration
- Runtime level changes (parseLevel, setGlobalLevel, setLoggerLevel)
- Named logger lookups
- Compile-time vs runtime gating
- Unit test reference

### 🔌 [engine-interface.txt](engine-interface.txt)
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

| Topic                      | Document                                  |
|----------------------------|-------------------------------------------|
| **First-time setup**       | BUILD_GUIDE.md                            |
| **Architecture overview**  | Architecture.md                           |
| **Project status**         | FrankyCPP_Codebase_Review.md              |
| **v1.x Enhancement plan**  | specs/V1_ENGINE_ENHANCEMENT_PLAN.md       |
| **v1.x Strength roadmap**  | specs/V1_ENGINE_STRENGTH_ROADMAP.md       |
| **Syzygy tablebases**      | specs/PLAN_Syzygy_Tablebase_Support.md    |
| **Engine Arena framework** | arena/README.md                           |
| **C++20 features**         | CPP20_Feature_Support.md                  |
| **Lazy SMP**               | Lazy_SMP_Explained.md                     |
| **Logging**                | Logger.md                                 |
| **UCI protocol**           | engine-interface.txt                      |
| **CLion IDE**              | CLion_WSL_Setup.md                        |

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

All documentation is current as of **2026-02-16**:
- ✅ Build instructions for all platforms
- ✅ Architecture and design documentation
- ✅ v1.x Enhancement roadmap and implementation plan
- ✅ Syzygy tablebase integration documented
- ✅ Technical references up to date
- ✅ IDE setup guides available
- ✅ Obsolete files removed

---

**Last updated:** 2026-02-16
