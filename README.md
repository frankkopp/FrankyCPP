# FrankyCPP_NewGen
Re-Start of C++ Version of Franky Chess Engine

Master
[![CMake](https://github.com/frankkopp/FrankyCPP/actions/workflows/cmake.yml/badge.svg)](https://github.com/frankkopp/FrankyCPP/actions/workflows/cmake.yml)
[![CodeQL](https://github.com/frankkopp/FrankyCPP/actions/workflows/codeql-analysis.yml/badge.svg?branch=master)](https://github.com/frankkopp/FrankyCPP/actions/workflows/codeql-analysis.yml)

dev_v0.5
[![CMake](https://github.com/frankkopp/FrankyCPP/actions/workflows/cmake.yml/badge.svg?branch=dev_v0.5)](https://github.com/frankkopp/FrankyCPP/actions/workflows/cmake.yml)
[![CodeQL](https://github.com/frankkopp/FrankyCPP/actions/workflows/codeql-analysis.yml/badge.svg?branch=dev_v0.5)](https://github.com/frankkopp/FrankyCPP/actions/workflows/codeql-analysis.yml)

## Version
v0.5 Enhanced eval

v0.5 Simple eval

v0.3 See FrankyGo - except Evaluation

v0.2 See FrankyGo - except Evaluation

---

## Build

FrankyCPP uses CMake with target-scoped includes and pinned third‑party libraries via FetchContent and vcpkg.

- C++ standard: C++20
- Supported (primary) platform: Windows/MSVC 2022 (static 3rd party via vcpkg; dynamic MSVC CRT)
- Generator: Ninja recommended (CLion config already uses Ninja)
- vcpkg triplet default on MSVC: `x64-windows-static-md`

### Pinned dependencies (kept for compatibility)
- fmt: 8.0.1
- spdlog: v1.10.0 (using external fmt)
- googletest: v1.13.0
- google benchmark: v1.7.1

These versions are intentionally pinned to avoid breaking changes; consider updates later with code changes.

### CMake options
- `FRANKYCPP_BUILD_TESTS` (ON): Build unit tests (GoogleTest)
- `FRANKYCPP_BUILD_BENCHMARKS` (ON): Build benchmarks (Google Benchmark)
- `FRANKYCPP_USE_PCH` (ON): Enable precompiled headers for FrankyCPPlib
- `STRICT_WARNINGS` (OFF): Treat warnings as errors (/WX) on MSVC
- `ENABLE_STD_EXECUTION` (ON): Define `HAS_EXECUTION_LIB` for code paths using `<execution>`
- `ENABLE_BMI2_PEXT` (ON): Define `HAS_PEXT`; on MSVC also adds `/arch:AVX2` to FrankyCPPlib
- `ENABLE_AVX2` (OFF): Adds `/arch:AVX2` to FrankyCPPlib explicitly
- `VCPKG_TARGET_TRIPLET` (x64-windows-static-md default on MSVC): Override vcpkg triplet if needed

Notes:
- Library PUBLIC deps (exposed via headers): `fmt`, `spdlog`, `Boost::serialization`.
- Executable-only dep: `Boost::program_options`.

### Quick build (Windows, cmd.exe)
If building outside an IDE, initialize the MSVC environment first.

1) Open “x64 Native Tools Command Prompt for VS 2022” (or run vcvars64.bat)
2) Configure (RelWithDebInfo example):

```
cmake -S D:\_DEV\FrankyCPP -B D:\_DEV\FrankyCPP\cmake-build-relwithdebinfo -G Ninja -DFRANKYCPP_BUILD_TESTS=ON -DFRANKYCPP_BUILD_BENCHMARKS=OFF
```

3) Build:

```
cmake --build D:\_DEV\FrankyCPP\cmake-build-relwithdebinfo -j 8
```

CLion users can just reload CMake; the IDE sets up the MSVC environment automatically.

### Artifacts
- App: `FrankyCPP_v<major>.<minor>`
- Install (Release/RelWithDebInfo): copies config/ and books/ next to the binary in `Release/bin`.
