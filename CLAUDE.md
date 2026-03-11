# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Platform

This is a **Windows** development environment. Use **PowerShell** syntax for all shell commands (e.g., `.\` for local executables, `;` to chain commands — NOT `&&`). If the Claude Code shell is bash, use Windows-compatible paths (e.g., `D:/_DEV/FrankyCPP`) and avoid Linux-only commands. When in doubt, prefer PowerShell or run commands through WSL explicitly.

---

## Code Change Authorization

**Only make code changes when explicitly requested or clearly implied by user intent.**

- **Allowed triggers:** "implement", "fix", "change", "update", "add", "remove", "refactor", "proceed", "go ahead", "do it"
- **Forbidden triggers:** "check", "show", "analyze", "review", "what", "how", "why", "explain", "wdyt", "should I", "any issues"
- **Gray areas:** If a user reports a problem without saying "fix it", ask first before modifying code.
- **Never commit** without explicit user permission.
- **Never create summary `.md` documents** unless explicitly requested; provide summaries in chat instead.

## Build Policy

**Do NOT attempt to compile/build using terminal commands.** Ask the user to build in CLion or run the build scripts themselves. After providing a fix, say: *"Please rebuild and test to verify the fix."*

If user explicitly requests a build command, the scripts are:
```powershell
# Windows
.\build_windows.ps1 release     # or: debug, relwithdebinfo, minsizerel

# Linux/WSL
./build_wsl.sh release gcc      # or: release clang
```

Build output directories: `cmake-build-win-release`, `cmake-build-win-debug`, `cmake-build-wsl-release`, etc.

---

## Running Tests

```powershell
# Run all tests (excluding slow ones) — Windows
.\cmake-build-win-release\test\FrankyCPP_v1.6_Test.exe -*SpeedTests.*:-*TimingTests.*

# Run a specific test suite
.\cmake-build-win-release\test\FrankyCPP_v1.6_Test.exe --gtest_filter=PositionTest.*
```

Tests auto-discovered by CMake; test files mirror source structure under `test/`.

---

## Architecture

### High-Level Flow

```
main.cpp → UciHandler → Search → Evaluator
                    ↘ Position     TT
                    ↘ Perft        OpeningBook
                                   Tablebase (Syzygy/Fathom)
```

`UciHandler` owns the command loop and `Search` instance. `Search` owns `TT`, `Evaluator`, `OpeningBook`, `Tablebase`, `History`, and a `plyStack` of `PlyInfo` (per-ply move generators + state).

### Source Modules (`src/`)

| Module         | Purpose                                                                                                      |
|----------------|--------------------------------------------------------------------------------------------------------------|
| `types/`       | Core zero-cost value types: `Bitboard`, `Move` (32-bit), `Square`, `Piece`, `Value`                          |
| `common/`      | `Logging.h` (spdlog), `ThreadPool.h`, `stringutil.h`                                                         |
| `chesscore/`   | `Position` (board state, make/unmake, Zobrist), `MoveGenerator`, `Perft`, `Values` (PSTs)                    |
| `config/`      | `ConfigRegistry` (single source of truth), `ConfigManager` (singleton), `SearchConfigData`, `EvalConfigData` |
| `engine/`      | `Search`, `PlyInfo`, `Evaluator`, `TT`, `PawnTT`, `See`, `UciHandler`, `UciOptions`, `SearchLimits`          |
| `openingbook/` | Book loading, querying, platform-specific Boost serialization cache                                          |
| `tablebase/`   | Fathom library interface (WDL/DTZ probing), path discovery, downloader                                       |
| `enginetest/`  | EPD test suite runner, search-tree analysis                                                                  |

### Search Algorithm

Iterative deepening + PVS (Principal Variation Search) with:
- Aspiration windows, null-move pruning (with verification), LMR, futility pruning, razoring
- Quiescence search with SEE pruning
- On-demand staged move ordering: PV move → TT move → Captures (MVV-LVA/SEE) → Killers → Counter move → Quiet (history-sorted)
- Syzygy tablebase probing at root (move filtering) and interior nodes (WDL)

### Threading Model

- **Main thread**: UCI command loop
- **Search thread (T0)**: Full iterative deepening, aspiration windows, time management, UCI output
- **Helper threads (T1..Tn)**: Full `iterativeDeepening()` (same code as T0, guarded by `isMainThread()`)
- **Timer thread**: time-limit enforcement
- `std::atomic_bool stopSearchFlag`, `std::binary_semaphore` for init/running state. TT is shared (lock-free).
- **Best-thread selection**: After search, `selectBestThread()` picks best result by depth+score across all threads.
- Configurable via `Threads`, `Best Thread Selection`, `Best Thread Score Margin` UCI options.

### Configuration System

Adding a new config option requires changes in **two linked places**:

1. **Struct member** in `src/config/SearchConfigData.h` or `EvalConfigData.h`:
   ```cpp
   bool USE_NEW_FEATURE = true;
   ```

2. **Registry entry** in `src/config/ConfigRegistry.cpp`:
   ```cpp
   {
     .name = "USE_NEW_FEATURE",
     .valueType = ConfigValueType::Bool,
     .domain = ConfigDomain::Search,
     .defaultValue = "true",
     .exposure = {.uci = true, .yaml = true, .display = true},
     .getter = [](const auto& s, const auto&) { return configToString(s.USE_NEW_FEATURE); },
     .setter = [](auto& s, auto&, const std::string& v) { s.USE_NEW_FEATURE = parseBool(v); }
   },
   ```

Access: `ConfigManager::instance().search().USE_NEW_FEATURE`

Override in tests:
```cpp
CONFIG_OVERRIDE_START()
  s.USE_PVS = false;
CONFIG_OVERRIDE_END();
```

**YAML convention:** Keys starting with `_` (underscore) in `config/search.yaml` and `config/eval.yaml` are reserved for internal/test use (e.g., `_YAML_SMOKE_TEST_MARKER`). The YAML parser silently skips them — no "unknown key" warning is emitted.

---

## Critical: Test Class Initialization

**Every test class using `MoveGenerator`, `Position`, or any chess move functionality MUST initialize attack tables.**

Without this, sliding piece moves (bishops, rooks, queens) will not be generated — tests will fail mysteriously.

```cpp
class YourTestClass : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();  // ← REQUIRED: initializes magic bitboards, Zobrist keys, etc.
    NEWLINE;
  }
};
```

---

## Code Style

- **Naming:** `PascalCase` classes, `camelCase` functions/variables, `UPPER_SNAKE_CASE` constants, `lowercase` namespaces
- **Header guards:** `#ifndef FRANKYCPP_*` pattern
- **Line length:** 120 characters max; Allman brace style for functions, attached for control structures
- **Const-correctness is mandatory:** local variables, parameters, range-for loops, and methods should be `const` by default; remove only if compiler requires
- **`[[nodiscard]]`** on functions where ignoring return value is likely a bug
- **Implementation file order** must mirror header declaration order
- Class layout: static constants → member fields → public methods → private methods → getters/setters
- **Commenting/documenting style:** Match the existing style in the codebase. See `src/engine/Search.h` as a reference example — header files use a banner block (`//===...`) with a high-level overview of the component (purpose, algorithm, key methods, usage), followed by `///` Doxygen-style comments on individual declarations with `@param`/`@return` tags. Study nearby files before adding new comments to stay consistent.

## Adding Source Files

1. Create `.cpp` / `.h` in appropriate `src/<module>/` directory — CMake auto-discovers via globbing
2. Use `FRANKYCPP_*` header guard pattern
3. Add copyright header: `// 2018-2026 Frank Kopp, MIT License`

## Git Commands (Windows)

Use `--no-pager` or limit output to prevent pager pauses:
```powershell
git --no-pager log -5 --oneline
git --no-pager diff --cached
git status --short
```

---

## Key Reference Docs

- `docs/Architecture.md` — detailed component diagrams and data-flow
- `docs/FrankyCPP_Codebase_Review.md` — comprehensive analysis and roadmap
- `docs/BUILD_GUIDE.md` — complete build instructions
- `docs/Logger.md` — logging system
- `docs/specs/` — feature planning documents (Configuration Refactor, Syzygy, QSearch, etc.)
