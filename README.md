# FrankyCPP_NewGen
Re-Start of C++ Version of Franky Chess Engine

Master
[![CMake](https://github.com/frankkopp/FrankyCPP/actions/workflows/cmake.yml/badge.svg)](https://github.com/frankkopp/FrankyCPP/actions/workflows/cmake.yml)
[![CodeQL](https://github.com/frankkopp/FrankyCPP/actions/workflows/codeql-analysis.yml/badge.svg?branch=master)](https://github.com/frankkopp/FrankyCPP/actions/workflows/codeql-analysis.yml)

dev_v0.6
[![CMake](https://github.com/frankkopp/FrankyCPP/actions/workflows/cmake.yml/badge.svg?branch=dev_v0.6)](https://github.com/frankkopp/FrankyCPP/actions/workflows/cmake.yml)
[![CodeQL](https://github.com/frankkopp/FrankyCPP/actions/workflows/codeql-analysis.yml/badge.svg?branch=dev_v0.6)](https://github.com/frankkopp/FrankyCPP/actions/workflows/codeql-analysis.yml)

## Version
v0.6 (in development)
v0.5 Enhanced eval and move to wrapper classes for Bitboard, Square, Move, etc.
v0.4 Simple eval
v0.3 See FrankyGo - except Evaluation
v0.2 See FrankyGo - except Evaluation

---

## Build

FrankyCPP uses CMake with target-scoped includes and third‑party libraries via vcpkg manifest mode.

- C++ standard: C++20
- Supported (primary) platform: Windows/MSVC 2022 (static 3rd party via vcpkg; dynamic MSVC CRT)
- Generator: Ninja recommended (CLion config already uses Ninja)
- vcpkg triplet default on MSVC: `x64-windows-static-md`

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
- Guard against dynamic vcpkg triplets on Windows: dynamic triplets (e.g., `x64-windows`) are rejected to ensure a DLL-free executable; use a `-static` triplet (default is `x64-windows-static-md`).
- LTO/IPO is enabled for Release/RelWithDebInfo/MinSizeRel when supported by the toolchain.
- Library links (PUBLIC): `Boost::serialization`, `Boost::program_options`. Logging uses spdlog header-only with std::format (no fmt linkage).

### Quick build (Windows, cmd.exe)
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
