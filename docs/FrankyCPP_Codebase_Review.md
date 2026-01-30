# FrankyCPP Codebase Review

## Executive Summary

FrankyCPP is a well-structured C++ chess engine (v0.7 in development) implementing the UCI protocol. It's a port/evolution from "FrankyGo" with modern C++20 features. The engine features alpha-beta search with advanced pruning techniques, a configurable evaluation function, opening book support, and comprehensive testing infrastructure.

---

## 1. BUILD APPROACH

### Current State

| Aspect | Details |
|--------|---------|
| **Build System** | CMake 3.16+ with Ninja generator recommended |
| **C++ Standard** | C++20 (enforced via `target_compile_features`) |
| **Package Management** | vcpkg manifest mode with pinned dependencies |
| **Primary Platform** | Windows/MSVC 2022 |
| **Dependencies** | Boost (program_options, serialization), spdlog (header-only), yaml-cpp, GoogleTest, Google Benchmark |
| **Triplet** | `x64-windows-static-md` (static 3rd party libs, dynamic MSVC CRT) |

### Strengths
- ✅ Well-documented `README.md` with build instructions
- ✅ vcpkg manifest with version overrides ensures reproducible builds
- ✅ Guard against accidental dynamic triplet usage on Windows
- ✅ LTO/IPO enabled for Release builds when supported
- ✅ CI pipeline with GitHub Actions (Ubuntu, Windows, macOS matrix)
- ✅ Precompiled headers (PCH) support for faster incremental builds
- ✅ Optional Unity/Jumbo builds for faster clean builds
- ✅ Separate build targets: main executable, tests, benchmarks

### Suggestions
1. **Update CI workflow version references** – The GitHub Actions workflow references `v0.5` executables while the project is at `v0.7`. Consider parameterizing the version or using CMake-generated names.

2. **Enable non-MSVC toolchains** – The CMakeLists.txt has commented-out GCC/Clang sections. Consider re-enabling them with modern flags (C++20, updated intrinsic handling) to simplify cross-platform development.

3. **Add CMake presets** – Consider adding `CMakePresets.json` for standardized configure/build workflows across IDEs and CI.

4. **Consider vcpkg binary caching** – For CI speed improvements, enable vcpkg binary caching to avoid rebuilding dependencies on every run.

5. **Add `ENABLE_SANITIZERS` option** – For Debug builds, consider an optional address/undefined behavior sanitizer flag for catching memory issues.

---

## 2. ARCHITECTURE

### High-Level Structure

```
FrankyCPP/
├── src/
│   ├── main.cpp           # Entry point, CLI parsing
│   ├── init.h             # Global initialization
│   ├── types/             # Core type definitions (16+ headers)
│   ├── common/            # Utilities (logging, threading, string utils)
│   ├── chesscore/         # Board representation, move generation
│   ├── engine/            # Search, evaluation, UCI handler
│   ├── enginetest/        # Test suite runner, tree size tests
│   └── openingbook/       # Opening book handling (SIMPLE, SAN, PGN)
├── test/                  # GoogleTest unit tests
├── testbench/             # Google Benchmark microbenchmarks
└── config/                # YAML configuration files
```

### Key Components

| Component | Responsibility |
|-----------|---------------|
| `UciHandler` | UCI protocol implementation, I/O management |
| `Search` | Alpha-beta with iterative deepening, aspiration windows, time management |
| `Evaluator` | Position evaluation with game-phase interpolation |
| `Position` | Board state, move execution/undo, Zobrist hashing |
| `MoveGenerator` | Legal/pseudo-legal move generation, phased on-demand generation |
| `TT` | Transposition table with PEXT-based indexing |
| `ConfigManager` | Singleton for YAML-based runtime configuration |
| `OpeningBook` | Opening book with serialized caching |

### Strengths
- ✅ Clean separation of concerns (types, core, engine, utilities)
- ✅ Modern C++20 value types (`Bitboard`, `Move`, `Square`) with zero-cost abstractions
- ✅ Singleton `ConfigManager` with lazy auto-initialization and override support
- ✅ Comprehensive logging infrastructure with compile-time level stripping
- ✅ Thread pool implementation for parallel operations
- ✅ Custom precompiled header strategy for common includes

### Suggestions
1. **Reduce header coupling** – Some headers like `types.h` aggregate many includes. Consider forward declarations or splitting into smaller focused headers to improve compile times.

2. **Dependency injection for testability** – `Search` directly owns `TT`, `Evaluator`, `OpeningBook`. Consider injecting these via constructor for easier mocking in tests.

3. **Abstract UCI I/O interface** – `UciHandler` already accepts streams for testing, but consider a formal interface (`IUciIO`) for cleaner test doubles.

4. **Consider `std::expected` for error handling** – The `ConfigManager::loadFromFiles()` returns `bool`. With C++23 (or a polyfill), `std::expected<void, Error>` would provide richer error information.

5. **Document thread safety guarantees** – `ConfigManager` notes it has no internal locking. Document expected usage patterns (single-threaded init) more prominently.

---

## 3. CODE STRUCTURE

### Type System

The `types/` directory contains well-designed value types:
- `Bitboard` – Zero-cost wrapper around `uint64_t` with RAII-style operations
- `Move` – 32-bit encoded move with embedded sort value
- `Square`, `File`, `Rank` – Strongly-typed position identifiers
- `Value`, `Score`, `Depth` – Semantic wrappers for search values

### Code Quality

| Aspect | Assessment |
|--------|------------|
| **Naming** | Consistent, descriptive (e.g., `generatePseudoLegalMoves`, `iterativeDeepening`) |
| **Comments** | Good class/method documentation, Doxygen-ready |
| **Testing** | Extensive GoogleTest coverage across all modules |
| **Benchmarks** | Dedicated benchmark suite for performance tracking |
| **Const-correctness** | Generally good, `[[nodiscard]]` used appropriately |
| **RAII** | Smart pointers used throughout (`unique_ptr`, `shared_ptr`) |

### Strengths
- ✅ `FRIEND_TEST` macro usage for test access without exposing internals
- ✅ Constexpr initialization for attack tables, LMR reduction tables
- ✅ Clean separation of config data structures from logic (`SearchConfigData`, `EvalConfigData`)
- ✅ Boost serialization for opening book caching
- ✅ Modern `std::format` for string formatting (via spdlog configuration)

### Suggestions
1. **Reduce raw pointer usage** – `UciHandler* uciHandler` in `Search` could be `std::weak_ptr` or a non-owning reference wrapper for clarity.

2. **Replace magic numbers** – Some constants like `UCI_UPDATE_INTERVAL = nanoPerSec` are defined inline. Consider grouping such constants in a dedicated configuration section.

3. **Add clang-format/clang-tidy configs** – Enforce code style consistency automatically. Some files show `// clang-format off` already – standardize the entire codebase.

4. **Consider modules (C++20)** – For very large projects, C++20 modules could further improve build times, though this is experimental with MSVC still.

5. **Improve error messages** – `FATAL_ERROR` on wrong vcpkg triplet is good; consider similar guards for missing config files with actionable messages.

6. **Use `std::span` for move lists** – Instead of returning `const MoveList*`, consider `std::span<const Move>` for safer, more modern API.

---

## 4. CHESS-SPECIFIC TOPICS

### Move Generation

| Feature | Implementation |
|---------|---------------|
| **Board Representation** | 8x8 piece array + Bitboards (hybrid) |
| **Sliding Attacks** | Magic bitboards with PEXT (BMI2 required) |
| **Move Generation** | Phased on-demand generation with stages |
| **Move Ordering** | PV move → TT move → Captures (MVV-LVA/SEE) → Killers → History → Quiet |

### Search

| Feature | Status |
|---------|--------|
| **Alpha-Beta** | ✅ Implemented |
| **Iterative Deepening** | ✅ With aspiration windows |
| **Principal Variation Search (PVS)** | ✅ Configurable |
| **Null Move Pruning** | ✅ With verification search, Zugzwang detection |
| **Late Move Reductions (LMR)** | ✅ Pre-computed table |
| **Quiescence Search** | ✅ With SEE pruning |
| **Transposition Table** | ✅ 16-byte entries, configurable size |
| **Killer Moves** | ✅ 2 slots per ply |
| **History Heuristic** | ✅ History + Counter moves |
| **Time Management** | ✅ Adaptive with complexity factor |
| **Pondering** | ✅ Supported |

### Evaluation

| Feature | Status |
|---------|--------|
| **Material** | ✅ Standard piece values |
| **Piece-Square Tables** | ✅ Mid-game and end-game tables |
| **Game Phase** | ✅ Tapered evaluation |
| **Pawn Structure** | ✅ Isolated, doubled, passed, phalanx, supported pawns |
| **Pawn Hash Table** | ✅ Dedicated pawn TT |
| **Mobility** | ✅ Knight, Bishop, Rook, Queen mobility |
| **King Safety** | ✅ Basic implementation |
| **Lazy Evaluation** | ✅ With configurable threshold |
| **NNUE** | ❌ Not implemented (classical eval only) |

### Strengths
- ✅ Very configurable search/eval via YAML (100+ parameters)
- ✅ Perft testing with on-demand vs full generation comparison
- ✅ EPD test suite runner for regression testing
- ✅ Opening book with multiple format support (SIMPLE, SAN, PGN)
- ✅ Book caching with version-aware serialization
- ✅ SEE (Static Exchange Evaluation) for capture ordering

### Suggestions

1. **Consider NNUE evaluation** – Modern engines use efficiently updatable neural networks. This is a significant undertaking but would dramatically improve playing strength.

2. **Add multi-threaded search (Lazy SMP)** – `ThreadPool` exists but search appears single-threaded. Adding Lazy SMP with shared TT would improve strength on multi-core systems.

3. **Implement Syzygy tablebase probing** – For endgame accuracy, Syzygy tablebases are invaluable. The Fathom library provides a clean C interface.

4. **Add singular extensions** – When a move is clearly the best (by a margin), extend its search depth to avoid missing critical lines.

5. **Consider counter-move history** – Expand history heuristic to include piece-to-square counter-move history for better move ordering.

6. **Add "capture-only" hash** – Some engines maintain a separate hash for quiescence to avoid polluting the main TT.

7. **Tune parameters with SPSA/Texel** – The many configurable parameters could benefit from automated tuning. Consider adding tuning infrastructure.

8. **Improve time management** – The complexity-based time allocation is good. Consider adding "best move instability" detection to allocate more time when the best move changes frequently.

9. **Validate PEXT availability at runtime** – Currently PEXT is a compile-time requirement. Consider software fallback for non-BMI2 CPUs (with performance warning).

10. **Benchmark against established engines** – Set up regular matches against Stockfish or other engines to track relative strength improvements.

---

## Summary Statistics

| Metric | Value |
|--------|-------|
| **Source Files** | ~50 `.cpp`/`.h` files in `src/` |
| **Test Files** | ~30 test files with comprehensive coverage |
| **Lines of Code** | ~15,000-20,000 (estimated) |
| **Dependencies** | 5 main (Boost, spdlog, yaml-cpp, GTest, GBench) |
| **Config Parameters** | 100+ tunable via YAML |

---

## Prioritized Recommendations

### Roadmap Overview

| Version | Focus | Timeline |
|---------|-------|----------|
| **v0.7 → v1.0** | Cleanup, documentation, quick improvements, build stability | Current sprint |
| **v1.x** | Major engine/strength improvements (Lazy SMP, tablebases, NNUE) | Future releases |

---

### v0.7 → v1.0: Cleanup & Quick Improvements

#### Cleanup Tasks

| # | Item | Effort | Complexity | Description |
|---|------|--------|------------|-------------|
| C1 | Consolidate ToDo files | 🟢 1-2 hours | 🟢 Low | Merge `ToDo.md` and `ToDo2.md` into single prioritized backlog or GitHub Issues |
| C2 | Update copyright years | 🟢 1 hour | 🟢 Low | Update "2018-2021" to "2018-2026" across all source files |
| C3 | Clean up commented code | 🟡 2-3 days | 🟢 Low | Review and remove/document commented-out code (e.g., GCC/Clang in CMakeLists.txt) |
| C4 | Standardize header guards | 🟢 1-2 hours | 🟢 Low | Verify all headers use consistent `#ifndef FRANKYCPP_*` pattern |
| C5 | Update CI version references | 🟢 1-2 hours | 🟢 Low | Replace hardcoded `v0.5` with CMake variables or workflow matrix parameters |

#### Documentation Tasks

| # | Item | Effort | Complexity | Description |
|---|------|--------|------------|-------------|
| D1 | Add Doxyfile | 🟢 2-4 hours | 🟢 Low | Configure Doxygen for the project, add to `.gitignore` for output |
| D2 | Create Architecture.md | 🟡 1-2 days | 🟢 Low | Document component relationships, data flow, threading model |
| D3 | Add Doxygen comments to public API | 🟡 3-5 days | 🟡 Medium | Add `@brief`, `@param`, `@return` to: `Position`, `Search`, `Evaluator`, `MoveGenerator`, `TT` |
| D4 | Document types/ headers | 🟡 2-3 days | 🟢 Low | Add file-level and class-level Doxygen for `Bitboard`, `Move`, `Square`, `Value`, etc. |
| D5 | Add CONTRIBUTING.md | 🟢 1-2 hours | 🟢 Low | Document code style, PR process, testing requirements |

#### Quick Improvements (Build/Code Quality)

| # | Item | Effort | Complexity | Description |
|---|------|--------|------------|-------------|
| Q1 | Add CMakePresets.json | 🟢 2-4 hours | 🟢 Low | Create preset file with common configurations (Debug, Release, CI) |
| Q2 | Add clang-tidy config | 🟢 2-4 hours | 🟢 Low | Create `.clang-tidy` file with appropriate checks for the codebase style |
| Q3 | Add `ENABLE_SANITIZERS` option | 🟢 2-4 hours | 🟢 Low | Optional ASan/UBSan for Debug builds to catch memory issues |
| Q4 | Enable GCC/Clang toolchains | 🟡 3-5 days | 🟡 Medium | Uncomment/update compiler sections, handle intrinsics, test on Linux/macOS |
| Q5 | Reduce header coupling | 🟡 1-2 weeks | 🟡 Medium | Audit includes, add forward declarations, split large headers |
| Q6 | Remove Stockfish references | 🟢 1-2 hours | 🟢 Low | Replace "from Stockfish" comments with proper technique citations due to GPL/MIT license incompatibility. Files: `macros.h`, `Search.cpp`. See: Razoring (chessprogramming.org), operator macros (common C++ pattern) |

---

### v1.x: Major Engine/Strength Improvements

| # | Item | Effort | Complexity | Description |
|---|------|--------|------------|-------------|
| E1 | Lazy SMP multi-threaded search | 🔴 High (2-4 weeks) | 🔴 High | Thread-safe TT, shared search state, careful synchronization. Major architectural change. |
| E2 | Syzygy tablebase support | 🟡 Medium (1-2 weeks) | 🟡 Medium | Integrate Fathom library, add probing at root and during search. |
| E3 | Singular extensions | 🟢 Low (2-3 days) | 🟡 Medium | Modify search to detect singular moves via reduced-depth verification. |
| E4 | Runtime PEXT fallback | 🟡 Medium (3-5 days) | 🟡 Medium | Add CPUID detection, implement software PEXT path for non-BMI2 CPUs. |
| E5 | Counter-move history | 🟡 Medium (3-5 days) | 🟡 Medium | Expand history heuristic to include piece-to-square counter-move history. |
| E6 | NNUE evaluation | 🔴 High (4-8 weeks) | 🔴 High | Implement efficiently updatable neural network evaluation. Major undertaking. |
| E7 | Parameter tuning infrastructure | 🟡 Medium (1-2 weeks) | 🟡 Medium | Add SPSA/Texel tuning framework for the 100+ configurable parameters. |
| E8 | Best-move instability time mgmt | 🟢 Low (2-3 days) | 🟡 Medium | Allocate more time when best move changes frequently between iterations. |
| E9 | Selective checks in quiescence | 🟡 Medium (3-5 days) | 🟡 Medium | Search quiet checking moves after capture phase to find short mates. |
| E10 | Check extensions | 🟢 Low (2-3 days) | 🟡 Medium | Extend +1 ply when a check leaves opponent with ≤2 legal replies. |

---

## 5. DOCUMENTATION & CLEANUP STATUS

### Current State Assessment

| Aspect | Status | Notes |
|--------|--------|-------|
| **README.md** | ✅ Good | Comprehensive build instructions, version history |
| **Code comments** | 🟡 Partial | Class-level comments exist, but inconsistent method documentation |
| **Doxygen** | ❌ Missing | No Doxyfile, no `@param`/`@return` tags in code |
| **docs/ folder** | 🟡 Fragmented | Multiple ToDo files (ToDo.md, ToDo2.md), some outdated content |
| **clang-format** | ✅ Present | `.clang-format` exists and is well-configured |
| **clang-tidy** | ❌ Missing | No `.clang-tidy` configuration file |
| **API documentation** | ❌ Missing | No generated HTML/PDF docs for library consumers |
| **Architecture docs** | 🟡 Partial | This review provides overview, but no dedicated architecture guide |

### Priority Order for v1.0 Documentation Tasks

1. **C1** - Consolidate ToDo files (quick win, reduces confusion)
2. **C2** - Update copyright years (trivial but important)
3. **D1** - Add Doxyfile (foundation for API docs)
4. **D2** - Create Architecture.md (helps onboarding)
5. **D3** - Document public API (most valuable for maintainability)
6. **D4** - Document types/ headers (improves code understanding)
7. **D5** - Add CONTRIBUTING.md (useful if accepting external contributions)

### Effort Legend
- 🟢 **Low**: Less than 1 day of focused work
- 🟡 **Medium**: 1 day to 2 weeks
- 🔴 **High**: More than 2 weeks

### Complexity Legend
- 🟢 **Low**: Straightforward, well-documented, minimal risk
- 🟡 **Medium**: Requires understanding of existing code, some design decisions
- 🔴 **High**: Architectural changes, threading/synchronization, significant testing needed

---

## Action Items Tracker

### v0.7 → v1.0 Tasks

#### Cleanup (C)

| # | Item | Effort | Status | Notes |
|---|------|--------|--------|-------|
| C1 | Consolidate ToDo files | 🟢 1-2 hours | ✅ DONE | Deleted ToDo.md + ToDo2.md (obsolete/implemented items) |
| C2 | Update copyright years | 🟢 1 hour | ✅ DONE | 2018-2021 → 2018-2026 (all source files) |
| C3 | Clean up commented code | 🟡 2-3 days | ⏭️ SKIP | TODOs are intentional reminders; CMake sections kept for Q4 |
| C4 | Standardize header guards | 🟢 1-2 hours | ✅ DONE | Converted 6 `#pragma once` to `FRANKYCPP_*` guards |
| C5 | Update CI version references | 🟢 1-2 hours | ⏭️ SKIP | GitHub CI not currently used; will fix when re-enabled |

#### Documentation (D)

| # | Item | Effort | Status | Notes |
|---|------|--------|--------|-------|
| D1 | Add Doxyfile | 🟢 2-4 hours | ⏭️ SKIP | Overkill; IDE navigation + inline comments sufficient |
| D2 | Create Architecture.md | 🟡 1-2 days | ✅ DONE | Component relationships, data flow, threading |
| D3 | Add Doxygen comments to public API | 🟡 3-5 days | ⏭️ SKIP | Same as D1; inline comments sufficient |
| D4 | Document types/ headers | 🟡 2-3 days | ✅ DONE | All 24 headers documented with purpose, deps, usage |
| D5 | Add CONTRIBUTING.md | 🟢 1-2 hours | ⏭️ SKIP | Single developer project; not needed |

#### Quick Improvements (Q)

| # | Item | Effort | Status | Notes |
|---|------|--------|--------|-------|
| Q1 | Add CMakePresets.json | 🟢 2-4 hours | ⬜ TODO | Standard CMake feature |
| Q2 | Add clang-tidy config | 🟢 2-4 hours | ⬜ TODO | Static analysis checks |
| Q3 | Add `ENABLE_SANITIZERS` option | 🟢 2-4 hours | ⬜ TODO | ASan/UBSan for Debug |
| Q4 | Enable GCC/Clang toolchains | 🟡 3-5 days | ⬜ TODO | Cross-platform testing |
| Q5 | Reduce header coupling | 🟡 1-2 weeks | ⬜ TODO | Incremental refactoring |
| Q6 | Remove Stockfish references | 🟢 1-2 hours | ⬜ TODO | GPL/MIT license fix in macros.h, Search.cpp |

---

### v1.x Engine/Strength Tasks

| # | Item | Effort | Status | Notes |
|---|------|--------|--------|-------|
| E1 | Lazy SMP multi-threaded search | 🔴 2-4 weeks | ⬜ TODO | Thread-safe TT, shared state |
| E2 | Syzygy tablebase support | 🟡 1-2 weeks | ⬜ TODO | Fathom library integration |
| E3 | Singular extensions | 🟢 2-3 days | ⬜ TODO | Localized search change |
| E4 | Runtime PEXT fallback | 🟡 3-5 days | ⬜ TODO | CPUID + fallback path |
| E5 | Counter-move history | 🟡 3-5 days | ⬜ TODO | Piece-to-square history |
| E6 | NNUE evaluation | 🔴 4-8 weeks | ⬜ TODO | Major undertaking |
| E7 | Parameter tuning infrastructure | 🟡 1-2 weeks | ⬜ TODO | SPSA/Texel tuning |
| E8 | Best-move instability time mgmt | 🟢 2-3 days | ⬜ TODO | Dynamic time allocation |
| E9 | Selective checks in quiescence | 🟡 3-5 days | ⬜ TODO | Search checking moves after captures |
| E10 | Check extensions | 🟢 2-3 days | ⬜ TODO | Extend when few legal replies to check |

---

*Review conducted: 2026-01-26*  
*Last updated: 2026-01-30*
