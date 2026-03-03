# Namespace Refactor Plan

**Status:** Not Started  
**Created:** 2026-03-03  
**Last Updated:** 2026-03-03 (rev 2)  
**Author:** Frank Kopp

---

## Project Metadata

| Field                | Value                                                        |
|----------------------|--------------------------------------------------------------|
| **Branch**           | `refactor/namespaces` (to be created)                        |
| **Risk Level**       | 🔴 High — touches virtually every source file                |
| **Estimated Effort** | 4–6 sessions (phases are independent, can be done over time) |
| **Rollback**         | Each phase is self-contained; revert the branch per phase    |

### Phase Status Tracker

| Phase                                | Status     | Notes                                                               |
|--------------------------------------|------------|---------------------------------------------------------------------|
| Phase 1: Engine & Config Layer       | ✅ Complete | Low risk, pure class/struct wrapping                                |
| Phase 2: Common & Book Layers        | ✅ Complete | Low risk, no enum constant fallout                                  |
| Phase 3: Arena & EngineTest Layers   | ✅ Complete | `arena::` + `enginetest::` both done                                |
| Phase 4: Chess Domain (Types + Core) | ✅ Complete | All sub-steps done: types/, chesscore/, fwd.h, consumer using-decls |
| Phase 5: `enum class` Strengthening  | ⏭️ Skipped | Plain enums preferred for chess engine ergonomics                   |

**Status Legend:** ⬜ Not Started | 🔄 In Progress | ✅ Complete | ⏭️ Skipped | ❌ Blocked

---

## Background and Motivation

### Current State

FrankyCPP has a **partially namespaced** codebase. Several utility namespaces already exist and work well:

| Existing Namespace | Location                 | Content                              |
|--------------------|--------------------------|--------------------------------------|
| `Attacks`          | `types/attacks.h`        | Magic bitboard lookup + `detail` sub |
| `Attacks::detail`  | `types/attacks.h`        | Internal helpers (hidden)            |
| `Bitboards`        | `types/bitboards.h`      | Precomputed bitboard tables          |
| `Squares`          | `types/square.h`         | Distance tables, square name array   |
| `Castling`         | `types/castlingrights.h` | Castling rights lookup table         |
| `MoveShifts`       | `types/movetype.h`       | Bit layout constants for `Move`      |
| `Values`           | `chesscore/Values.h`     | Piece values, piece-square tables    |
| `Types`            | `types/init.h`           | Type system init wrapper             |
| `init`             | `src/init.h`             | Top-level init wrapper               |
| `tablebase`        | `tablebase/*.cpp/.h`     | Syzygy tablebase probing             |

Everything else lives in the **global namespace**: all core type classes (`Color`, `Square`, `Piece`, `Move`, `Value`, ...), all enum constants (`WHITE`, `BLACK`, `KING`, `NORTH`, `SQ_E4`, ...), all engine classes (`Search`, `Evaluator`, `TT`, `UciHandler`, ...), config, common utilities, and the opening book.

### Why This Is a Problem

1. **Name collision risk** — constants like `KING`, `QUEEN`, `WHITE`, `BLACK`, `NORTH`, `EAST`, `WEST`, `NONE` are extremely common English words. Any external library or future module could silently clash.
2. **Fragile `str()` overloads** — six-plus global `str()` overloads distinguished only by argument type, on types that all define `operator int()`. Overload resolution could silently pick the wrong one.
3. **Internal inconsistency** — `Attacks::attacks(ROOK, sq, occ)` is already written where `ROOK`, `sq`, and `occ` are all global. The namespace pattern is known and used; it just wasn't applied systematically.
4. **ADL reliability** — `operator<<` and other free functions work by ADL today, but only because all types and operators share the global namespace. Moving to namespaces will make ADL deterministic.
5. **IDE / tooling clarity** — `engine::Search` and `chess::Move` are unambiguous; bare `Search` and `Move` are not.

---

## Top-Level Namespace Question: `frankycpp::` Wrapper or Not?

### Option A — Single top-level `frankycpp::` namespace

```cpp
namespace frankycpp {
    namespace chess { ... }
    namespace engine { ... }
    namespace config { ... }
    namespace book { ... }
    namespace common { ... }
    namespace tablebase { ... }  // already exists, would move inside
}
```

**Pros:**
- Completely isolated from all other libraries — zero collision risk even with common sub-namespace names like `engine::` or `config::` used by other libraries.
- Clean public API surface if FrankyCPP were ever used as a library.
- Uniform prefix; `frankycpp::chess::Move` leaves no ambiguity.

**Cons:**
- Verbose. Internal code would need `using namespace frankycpp::chess;` everywhere, or two levels of `using`.
- The engine is a standalone binary, not a library. The isolation benefit is theoretical.
- `tablebase::` (already in use) would need to become `frankycpp::tablebase::` — breaking existing code.
- Adds a mechanical wrapping step to every file with no practical gain for a single-binary project.

### Option B — Folder-derived namespaces (no outer wrapper) ✅ Recommended

Map each source folder directly to a namespace:

| Source Folder       | Namespace       | Content                                     |
|---------------------|-----------------|---------------------------------------------|
| `src/types/`        | `chess::` *     | Core types, constants, bitboard tables      |
| `src/chesscore/`    | `chess::` *     | Position, MoveGenerator, History, Perft     |
| `src/engine/`       | `engine::`      | Search, Evaluator, TT, UciHandler, etc.     |
| `src/config/`       | `config::`      | ConfigManager, ConfigRegistry, data structs |
| `src/common/`       | `common::`      | Logger, ThreadPool, CrashHandler            |
| `src/openingbook/`  | `book::`        | OpeningBook, BookEntry                      |
| `src/tablebase/`    | `tablebase::` ✅ | Already correct — keep as-is                |
| `src/enginetest/`   | `enginetest::`  | TestSuite, EpdParser                        |
| `src/engine_arena/` | `arena::`       | ArenaRunner, MatchRunner, etc.              |

\* `types/` and `chesscore/` are intentionally merged into one `chess::` namespace — they are a single cohesive chess domain. The directory split is an implementation detail, not a semantic boundary.

**Why folder-derived over a `frankycpp::` wrapper:**
- The engine is a standalone binary — there is no external consumer that needs namespace isolation from `engine::` or `config::` names.
- Existing `tablebase::` stays valid — no breakage.
- Two-level scoping (`engine::Search`) is readable without being verbose.
- Matches the mental model already present in the codebase (the folder structure *is* the architecture).
- Consistent with how projects like Stockfish handle this (`Stockfish::` is never used).

---

## Proposed Namespace Map (Detailed)

### `chess::` — types + chesscore merged

```cpp
namespace chess {
    // --- primitives (types/) ---
    class Color;
    class File;
    class Rank;
    class Square;
    class Direction;
    class Bitboard;
    class Value;
    class Depth;          // (currently a bare enum)
    class Piece;          // (currently a bare enum)
    class PieceType;      // (currently a bare enum)
    class MoveType;       // (currently a bare enum)
    class CastlingRights;
    class ZobristKey;
    class Move;
    class Score;

    // inline constants (all the SQ_A1..SQ_H8, WHITE, BLACK, KING..QUEEN, NORTH..., etc.)
    inline constexpr Color WHITE{0};
    inline constexpr Color BLACK{1};
    // ... etc.

    // sub-namespaces (already exist, kept or renamed consistently)
    namespace bb { ... }         // was Bitboards::
    namespace attacks { ... }    // was Attacks::
    namespace squares { ... }    // was Squares::
    namespace castling { ... }   // was Castling::

    // --- chess core (chesscore/) ---
    class Position;
    class MoveGenerator;
    class History;
    class MoveUtils;
    class Perft;
    enum GenMode { GenZero, GenNonQuiet, GenQuiet, GenAll };

    // evaluation tables (chesscore/Values.h)
    namespace values { ... }     // was Values::
}
```

### `engine::`

```cpp
namespace engine {
    class Search;
    class Evaluator;
    class TT;
    class PawnTT;
    class UciHandler;
    class UciOptions;
    class See;
    class Benchmark;
    struct SearchLimits;
    struct SearchResult;
    struct SearchStats;
    struct SearchThreadData;
    struct PlyInfo;
    struct PVTable;
}
```

### `config::`

```cpp
namespace config {
    class ConfigManager;
    class ConfigRegistry;
    struct ConfigDef;
    struct SearchConfigData;
    struct EvalConfigData;
    enum class ConfigDomain;
    enum class ConfigValueType;
    enum class ConfigMode;
}
```

### `common::`

```cpp
namespace common {
    class Logger;
    class ThreadPool;
    class CrashHandler;
    // misc.h, stringutil.h, stacktrace.h utilities
}
```

### `book::`

```cpp
namespace book {
    class OpeningBook;
    struct BookEntry;
}
```

### `tablebase::` — already correct, no change needed ✅

### `enginetest::`

```cpp
namespace enginetest {
    class TestSuite;
    class EpdParser;
    struct TestTypes;
}
```

### `arena::`

```cpp
namespace arena {
    class ArenaRunner;
    class MatchRunner;
    class BenchmarkRunner;
    class TestSuiteRunner;
    class UCIEngine;
    class ResultWriter;
    struct ArenaConfig;
    struct ArenaResults;
}
```

---

## Migration Strategy

### The Core Challenge: `chess::` Constants

The biggest migration challenge is the hundreds of global constants that are used in virtually every file:

```cpp
// Currently global — used everywhere:
WHITE, BLACK, SQ_E4, SQ_NONE, KING, PAWN, KNIGHT, BISHOP, ROOK, QUEEN,
NORTH, SOUTH, EAST, WEST, NORTH_EAST, NORTH_WEST, SOUTH_EAST, SOUTH_WEST,
FILE_A..FILE_H, RANK_1..RANK_8, DEPTH_ONE..DEPTH_MAX,
MOVE_NONE, VALUE_ZERO, VALUE_INF, VALUE_NONE, ...
```

**The mitigation: Stockfish pattern**

Implementation (`.cpp`) files in `src/types/`, `src/chesscore/`, `src/engine/` etc. simply add:

```cpp
using namespace chess;
```

at file scope, below the includes. This means:
- All internal engine code continues to compile unchanged.
- The namespace exists and provides clean `chess::Move`, `chess::Square` etc. for any external consumer or new code.
- No forced mass-rename of every constant at every call site.

Headers *should not* carry `using namespace chess;` at file scope (that would pollute includers). Headers reference the full `chess::Move`, `chess::Square` etc. — but since the headers are primarily included by `.cpp` files that have the `using` declaration, the cascade effect is contained.

Tests follow the same pattern: add `using namespace chess;` in test `.cpp` files.

### Sub-namespace Rename Policy

The existing sub-namespaces under `chess::` will be **lowercased** to match the new scheme:

| Old Name     | New Name             | Action                                                       |
|--------------|----------------------|--------------------------------------------------------------|
| `Bitboards`  | `chess::bb`          | Rename + move inside `chess`                                 |
| `Attacks`    | `chess::attacks`     | Rename + move inside `chess`                                 |
| `Squares`    | `chess::squares`     | Rename + move inside `chess`                                 |
| `Castling`   | `chess::castling`    | Rename + move inside `chess`                                 |
| `MoveShifts` | `chess::move_shifts` | Rename + move inside `chess`                                 |
| `Values`     | `chess::values`      | Rename + move inside `chess`                                 |
| `Types`      | (absorbed)           | `Types::init()` → `chess::init()` or kept as `types::init()` |
| `init`       | (kept as-is)         | Top-level `init::init()` is fine                             |

To avoid breaking all call sites during transition, add inline aliases in the old headers:

```cpp
namespace Bitboards = chess::bb;      // compatibility alias during migration
namespace Attacks = chess::attacks;   // remove once all callers updated
```

---

## Implementation — Phase Details

### Phase 1: Engine & Config Layers

**Scope:** `src/engine/`, `src/config/`  
**Risk:** 🟢 Low — pure class/struct wrapping, no enum constant cascade  
**Files affected:** ~25 headers + their `.cpp` files  

Steps:
1. Wrap all `src/engine/` headers in `namespace engine { ... }`.
2. Wrap all `src/config/` headers in `namespace config { ... }`.
3. Add `using namespace engine;` / `using namespace config;` to the corresponding `.cpp` files.
4. Update all `#include` users that reference these types by name (forward declarations, `std::unique_ptr<Search>` → `std::unique_ptr<engine::Search>`).
5. Fix `UciHandler` constructor — it aggregates `Position`, `MoveGenerator` (chess layer) and `Search` (engine layer); forward declarations need qualified names.
6. Verify: build, run all tests.

Key callout — `ConfigManager::instance()` is used via `ConfigManager::instance().search()` everywhere. This becomes `config::ConfigManager::instance()`. The `using namespace config;` in `.cpp` files handles this for implementation files; only the engine entry point (`main.cpp`) needs explicit qualification or its own `using`.

---

### Phase 2: Common & Book Layers

**Scope:** `src/common/`, `src/openingbook/`  
**Risk:** 🟢 Low  
**Files affected:** ~15 headers + `.cpp` files  

Steps:
1. Wrap `src/common/` in `namespace common { }`.
2. Wrap `src/openingbook/` in `namespace book { }`.
3. Add `using namespace common;` / `using namespace book;` to corresponding `.cpp` files.
4. `Logging.h` macros (`LOG__INFO`, etc.) are preprocessor macros — they cannot be namespaced and require no change.
5. Logger singletons (`Logger::get().SEARCH_LOG`) become `common::Logger::get().SEARCH_LOG` — covered by `using namespace common;` in engine `.cpp` files.
6. Verify: build, run all tests.

---

### Phase 3: Arena & EngineTest Layers ✅

**Scope:** `src/engine_arena/`, `src/enginetest/`  
**Risk:** 🟢 Low — these are standalone executables / test infrastructure  
**Status:** ✅ Complete (2026-03-03)

**Pre-existing state:** All 8 `src/engine_arena/` headers were already wrapped in `namespace arena { }`.

**Completed:**
1. ~~Wrap `src/engine_arena/` in `namespace arena { }`.~~ ✅ Already complete.
2. ✅ Wrapped `src/enginetest/` in `namespace enginetest { }` — covers `TestTypes.h`, `EdpTest.h`, `EpdParser.h`, `TestSuite.h`, `SearchTreeSizeTest.h` (including `SearchTreeSize` sub-namespace).
3. ✅ Added `using namespace enginetest;` in all `.cpp` implementation files.
4. ✅ Updated cross-module consumers: `main.cpp`, `TestSuiteRunner.h/.cpp` (arena).
5. ✅ Updated all 5 enginetest test files with `using namespace enginetest;`.

---

### Phase 4: Chess Domain (Types + Core) — Merged

**Scope:** `src/types/` + `src/chesscore/` — both map to `chess::`  
**Risk:** 🟡 Medium (mitigated by `using namespace chess;`)  
**Files affected:** All type headers, all chesscore headers, every `.cpp` file in the project  

**Rationale for merging:** `types/` and `chesscore/` share the target namespace `chess::`. Doing them separately creates an awkward intermediate state where `Position` is in `chess::` but `Square`, `Move`, etc. are still global — or vice versa. A single phase avoids that inconsistency.

#### Macro Strategy — `ENABLE_*` Operator Invocations

The `ENABLE_*_OPERATORS_ON` macros in `macros.h` produce free functions (e.g., `operator++`, `operator+`). For ADL to find these operators on types inside `namespace chess {}`, the macro invocations **must be placed inside the `namespace chess {}` block** in each type header. The macros themselves (`macros.h`) remain unchanged — they are preprocessor definitions and cannot be namespaced.

Example — `color.h` after migration:
```cpp
namespace chess {
  class Color { ... };

  inline constexpr Color WHITE{0};
  inline constexpr Color BLACK{1};
  inline constexpr Color NOCOLOR{2};

  constexpr Color operator~(const Color c) { return Color{(c.value() ^ 1U)}; }
  ENABLE_INCR_OPERATORS_ON(Color)     // ← inside namespace chess for ADL
  ENABLE_OSTREAM_OPERATOR_AS_INT_ON(Color)
} // namespace chess

// std::formatter specialization — outside namespace chess (lives in namespace std)
ENABLE_FORMATTER_AS_CHAR_ON(chess::Color);
```

#### Macro Strategy — `ENABLE_FORMATTER_*` Specializations

`ENABLE_FORMATTER_*_ON` macros produce `template<> struct std::formatter<T>` — these must live **outside** `namespace chess {}` (they are `namespace std` specializations). The type argument must be fully qualified.

**Approach:** Move formatter macro invocations outside the namespace block and qualify the type with `chess::`:
```cpp
} // namespace chess
ENABLE_FORMATTER_AS_STRING_VIEW_ON(chess::Square);
ENABLE_FORMATTER_AS_INT_ON(chess::Value);
```

No new wrapper macros needed — explicit `chess::` prefix is simpler and more transparent.

#### Steps

1. **Types first** — Wrap each type header in `namespace chess { }`:
   - `color.h`, `file.h`, `rank.h`, `square.h`, `direction.h`, `orientation.h`
   - `bitboard.h`, `bitboards.h`
   - `piece.h`, `piecetype.h`, `value.h`, `depth.h`, `score.h`
   - `move.h`, `movetype.h`, `staticmovelist.h`
   - `castlingrights.h`, `zobristkey.h`
   - `globals.h` constants (`START_POSITION_FEN`, `MAX_DEPTH`, etc.)
   - `attacks.h`, `attacks.cpp`
2. **Move `ENABLE_*` operator macro invocations inside `namespace chess {}`** in each type header. Move `ENABLE_FORMATTER_*_ON` invocations *outside* the namespace block with `chess::` qualified type name.
3. **Chesscore** — Wrap `src/chesscore/` headers in `namespace chess { }`:
   - `Position.h/.cpp`, `MoveGenerator.h/.cpp`, `History.h/.cpp`
   - `MoveUtils.h/.cpp`, `Perft.h/.cpp`, `Zobrist.h`, `Values.h`
4. Rename sub-namespaces as per the rename table (`Bitboards` → `chess::bb`, etc.).
5. Add compatibility aliases (`namespace Bitboards = chess::bb;`) and deprecate over a few sessions.
6. Add `using namespace chess;` to all `.cpp` files that use chess constants directly.
7. `types/types.h` (the aggregate include) adds `using namespace chess;` at the bottom — this propagates the `using` to all legacy code that does `#include "types/types.h"`.

   > ⚠️ **Note:** Putting `using namespace` in a header is normally a bad practice. Here it is acceptable as a **transitional measure only** in the aggregate convenience header `types.h`, clearly commented. The goal is to remove it once all `.cpp` files have their own explicit `using` and the header usage is cleaned up.

8. Introduce `src/chesscore/fwd.h` with forward declarations:
   ```cpp
   namespace chess {
       class Position;
       class MoveGenerator;
       class Move;
       // etc.
   }
   ```
   Replace bare forward declarations in engine headers (`UciHandler.h`, `See.h`, `Perft.h`, `MoveUtils.h`, `MoveGenerator.h`) with `#include "chesscore/fwd.h"`.
9. Update `boost::serialization` specialization in `move.h` — fully qualify `chess::Move`.
10. Verify: build, run all tests, run perft suite.

---

### Phase 5: `enum class` Strengthening — ⏭️ Skipped

**Decision:** Skipped. Plain enums are idiomatic and practical in a chess engine — `PieceType`, `Piece`, `Depth`, and `MoveType` are used heavily in arithmetic, array indexing, and bit manipulation. Converting to `enum class` would require explicit casts at hundreds of call sites, adding boilerplate without meaningful safety gain. The existing class wrappers (`Color`, `Square`, `File`, `Rank`, etc.) already provide type safety where it matters most.

---

## Files That Do Not Change

| File/Category                 | Reason                                                  |
|-------------------------------|---------------------------------------------------------|
| `src/types/macros.h`          | Preprocessor macros cannot be namespaced                |
| `src/common/Logging.h` macros | `LOG__INFO` etc. are preprocessor macros                |
| `fathom/` (Fathom C library)  | External C code — no namespace change                   |
| `CMakeLists.txt`              | Build system — unaffected                               |
| `config/*.yaml`               | YAML config files — unaffected                          |
| Header guards (`#ifndef`)     | Stay as `FRANKYCPP_*` — unrelated to runtime namespaces |

---

## Testing Strategy

- After each phase: run the full test suite (`FrankyCPP_v*_Test.exe`).
- After Phase 4: run a perft suite to confirm `Position` / `MoveGenerator` behaviour is unchanged.
- After Phase 4: run a short benchmark and compare NPS — namespaces are compile-time and should have zero runtime impact.
- Compile with both MSVC (primary) and clang (if available) to catch ADL differences.

---

## Risk Register

| Risk                                                   | Likelihood | Impact | Mitigation                                                                     |
|--------------------------------------------------------|------------|--------|--------------------------------------------------------------------------------|
| ADL breaks `operator<<` for chess types                | Medium     | Low    | Operators defined in `chess::` — ADL finds them                                |
| `ENABLE_*` operator macros outside `chess::` break ADL | High       | High   | Place all operator macro invocations **inside** `namespace chess {}` blocks    |
| `ENABLE_FORMATTER_*` macros need qualified types       | High       | Medium | Move formatter macro invocations outside namespace block with `chess::` prefix |
| `boost::serialization` specialization breaks           | Low        | Medium | Fully qualify `chess::Move` in the specialization                              |
| `using namespace chess;` in `types.h` leaks            | Medium     | Low    | Document as transitional; remove in cleanup pass                               |
| Test files not updated — compile errors                | High       | Low    | Add `using namespace chess;` to test `.cpp` files                              |
| `FRIEND_TEST` macros in chess classes                  | Low        | Low    | `FRIEND_TEST` uses class names, not namespaces                                 |

---

## Non-Goals

- **No renaming of types** — `Position` stays `Position`, `Search` stays `Search`; only the namespace wrapper changes.
- **No change to file names or directory structure** — folders stay as-is; they just map to namespaces.
- **No change to the build system** — CMake globs remain valid.
- **No change to public API contracts** — UCI protocol output, FEN parsing, PGN format, etc. are unaffected.
- **No top-level `frankycpp::` wrapper** — rejected as unnecessary overhead for a standalone binary (see decision above).
