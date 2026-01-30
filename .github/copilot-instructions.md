# FrankyCPP Copilot Instructions

---

## ⚠️ Code Change Authorization Rules

**CRITICAL: Only make code changes when explicitly requested or clearly implied by user intent**

### When CODE CHANGES are allowed:
- User uses action verbs: "implement", "fix", "change", "update", "add", "remove", "refactor", "proceed"
- User gives explicit permission: "go ahead", "do it", "make the changes"
- User provides specific requirement: "I want X to do Y", "Change X to Y"
- Follow-up after analysis: User reviews findings and says "fix it", "apply the fix"

### When CODE CHANGES are FORBIDDEN:
- User requests information: "check", "show", "analyze", "review", "what", "how", "why", "explain", "wdyt"
- User asks for opinion: "should I", "what do you think", "is this correct", "any issues"
- User requests analysis: "look at the logs", "check if", "verify", "investigate"
- User says "no code change" explicitly
- **Exception:** Read-only operations are ALWAYS allowed (read files, grep, check logs, query API)

### Gray areas - ASK FIRST:
- User reports a problem but doesn't explicitly say "fix it"
- User shows logs/errors without requesting action
- Ambiguous requests like "thoughts?" or "what about this?"

---

## Project Overview

FrankyCPP is a UCI chess engine written in modern C++20. It features alpha-beta search with advanced pruning, a classical evaluation function, opening book support, and comprehensive testing infrastructure.

---

## Build Environment

### Platform & Toolchain
- **Primary Platform**: Windows 11 with MSVC 2022 (Visual Studio 17.x)
- **IDE**: CLion with MSVC toolchain
- **Build System**: CMake 3.16+ with Ninja generator
- **Package Manager**: vcpkg in manifest mode (`vcpkg.json`)
- **C++ Standard**: C++20 (enforced globally)

### Key Build Commands
```powershell
# Configure (from project root)
cmake -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build cmake-build-release --config Release

# Run tests
cmake --build cmake-build-release --target FrankyCPP_v0.7_Test
.\cmake-build-release\test\FrankyCPP_v0.7_Test.exe
```

### Dependencies (via vcpkg)
- Boost (program_options, serialization)
- spdlog (header-only logging)
- yaml-cpp (configuration files)
- GoogleTest (unit testing)
- Google Benchmark (performance benchmarks)

---

## Code Style & Conventions

### Naming Conventions
- **Classes**: `PascalCase` (e.g., `MoveGenerator`, `SearchConfig`)
- **Functions/Methods**: `camelCase` (e.g., `generatePseudoLegalMoves`, `getZobristKey`)
- **Variables**: `camelCase` (e.g., `searchDepth`, `bestMove`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `MAX_PLY`, `VALUE_INFINITE`)
- **Namespaces**: `lowercase` (e.g., `engine::config`)
- **File names**: `PascalCase.cpp/.h` for classes, `lowercase.h` for type headers

### Header Guards
Use `#ifndef FRANKYCPP_*` pattern consistently:
```cpp
#ifndef FRANKYCPP_POSITION_H
#define FRANKYCPP_POSITION_H
// ...
#endif // FRANKYCPP_POSITION_H
```

### Code Formatting
- `.clang-format` is present in the project root - follow it
- Use `// clang-format off` only for alignment-sensitive tables (piece-square tables, etc.)
- Maximum line length: 120 characters
- Braces: Allman style for functions, attached for control structures

### Modern C++ Guidelines
- Prefer `constexpr` for compile-time constants
- Use `[[nodiscard]]` for functions where ignoring return value is likely a bug
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw pointers for ownership
- Use `std::string_view` for non-owning string parameters
- Prefer range-based for loops
- Use structured bindings where appropriate

---

## Architecture Overview

### Directory Structure
```
src/
├── types/          # Core value types (Bitboard, Move, Square, Value, etc.)
├── common/         # Utilities (Logging, ThreadPool, string utils)
├── chesscore/      # Board representation, move generation, position
├── engine/         # Search, evaluation, UCI handler, TT
├── enginetest/     # Test suite runner
└── openingbook/    # Opening book handling
```

### Key Classes
| Class | Responsibility |
|-------|----------------|
| `Position` | Board state, move execution, Zobrist hashing |
| `MoveGenerator` | Legal/pseudo-legal move generation |
| `Search` | Alpha-beta search with iterative deepening |
| `Evaluator` | Position evaluation |
| `TT` | Transposition table |
| `UciHandler` | UCI protocol implementation |
| `ConfigManager` | YAML configuration management (singleton) |

### Configuration System
- Runtime config via YAML files in `config/` directory
- `SearchConfigData` and `EvalConfigData` structs hold all parameters
- Access via `ConfigManager::instance().search()` and `.eval()`
- Override in tests using `CONFIG_OVERRIDE_START()` / `CONFIG_OVERRIDE_END()` macros

---

## Testing

### Test Framework
- GoogleTest for unit tests
- Google Benchmark for performance tests
- Tests mirror source structure in `test/` directory

### Running Tests
```powershell
# Run all tests (excluding slow ones)
.\cmake-build-release\test\FrankyCPP_v0.7_Test.exe -*SpeedTests.*:-*TimingTests.*

# Run specific test
.\cmake-build-release\test\FrankyCPP_v0.7_Test.exe --gtest_filter=PositionTest.*
```

### Writing Tests
- Use `FRIEND_TEST` macro for testing private members
- Place test files in corresponding `test/<module>/` directory
- Use `Test_Fens.h` for standard test positions

---

## Chess-Specific Notes

### Move Representation
- `Move` is a 32-bit value encoding: from, to, move type, promotion piece, sort value
- Use `Move::str()` for UCI notation (e.g., "e2e4", "e7e8q")
- `MOVE_NONE` represents no move / invalid move

### Bitboard Operations
- `Bitboard` wraps `uint64_t` with chess-specific operations
- Uses PEXT (BMI2) for sliding piece attacks - requires compatible CPU
- Square mapping: a1=0, h1=7, a8=56, h8=63 (little-endian rank-file)

### Search
- Iterative deepening with aspiration windows
- PVS (Principal Variation Search) with null-move pruning, LMR
- Time management via `SearchLimits` structure
- Search can be stopped via `stopSearch()` or time limit

---

## Windows & PowerShell Notes

### General
- Assume Windows PowerShell for all shell commands
- Do not chain commands with `&&` or `;` - use separate code blocks
- Use `.\` prefix to run local executables
- Do not attempt to compile code - ask user to compile in CLion and report errors

### Environment Variables
```powershell
# Session only
$env:VAR = "value"

# Persistent (user level)
[System.Environment]::SetEnvironmentVariable('VAR','value','User')
```

---

## Common Tasks

### Adding a New Source File
1. Create `.cpp` and `.h` in appropriate `src/<module>/` directory
2. Add header guard using `FRANKYCPP_*` pattern
3. Add copyright header (2018-2026 Frank Kopp, MIT License)
4. File is auto-discovered by CMake globbing

### Adding a New Test
1. Create test file in `test/<module>/` directory
2. Include `<gtest/gtest.h>` and relevant headers
3. Use `TEST()` or `TEST_F()` macros
4. Tests are auto-discovered by CMake

### Modifying Configuration
1. Add new field to `SearchConfigData` or `EvalConfigData` with default value
2. Add YAML parsing in the `convert<>` specialization
3. Update `config/search.yaml` or `config/eval.yaml` with new parameter
4. Access via `ConfigManager::instance().search().NEW_FIELD`

---

## Documentation Reference

- `docs/FrankyCPP_Codebase_Review.md` - Comprehensive codebase analysis and roadmap
- `docs/Logger.md` - Logging system documentation
- `README.md` - Build instructions and version history

---

*Last updated: 2026-01-30* 
