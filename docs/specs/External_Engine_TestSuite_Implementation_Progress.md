# External Engine Test Suite Support - Implementation Progress

## Status: IN PROGRESS

**Started:** 2026-02-02
**Last Updated:** 2026-02-03 (Phase 4 Complete)

**Related Documents:**
- `External_Engine_TestSuite_Support_Outline.md` - Main implementation plan
- `External_Engine_TestSuite_Support_Discussion.md` - Design decisions

---

## Phase Completion Status

### ✅ Phase 0: EPD Parser Extraction (COMPLETE)
**Completed:** 2026-02-01
**Time:** ~3 hours

**Tasks Completed:**
- [x] Extracted EPD parsing logic to `src/common/EPDParser.{h,cpp}`
- [x] Refactored `TestSuite` to use shared parser
- [x] All existing tests pass with no behavioral changes
- [x] Created comprehensive unit tests for EPDParser

**Files Modified:**
- Created: `src/common/EPDParser.h`
- Created: `src/common/EPDParser.cpp`
- Created: `test/common/EPDParser_Test.cpp`
- Modified: `src/enginetest/TestSuite.h`
- Modified: `src/enginetest/TestSuite.cpp`
- Modified: `src/CMakeLists.txt`

**Validation:**
- All existing TestSuite tests pass unchanged
- EPDParser unit tests cover all test types (BM, AM, DM)
- Handles edge cases and malformed input

---

### ✅ Phase 1: Configuration (COMPLETE)
**Completed:** 2026-02-02
**Time:** ~30 minutes

**Tasks Completed:**
- [x] Added `enginePath` field to `TestSuiteConfig` struct in `ArenaConfig.h`
- [x] Added `isolatePositions` field to `TestSuiteConfig` (default: true)
- [x] Updated YAML parsing in `ArenaConfig.cpp` to read optional `enginePath` and `isolatePositions`
- [x] Added validation: checks `enginePath` exists if non-empty
- [x] Updated `arena.yaml` with comprehensive examples and documentation
- [x] Added informational output to show internal vs external engine mode

**Files Modified:**
- Modified: `src/engine_arena/ArenaConfig.h`
- Modified: `src/engine_arena/ArenaConfig.cpp`
- Modified: `src/engine_arena/TestSuiteRunner.cpp`
- Modified: `config/arena.yaml`

**Configuration Design:**
- `enginePath` is an optional field (empty = internal engine, non-empty = external UCI engine)
- `isolatePositions` controls whether to clear engine state between positions:
  - `true` (default): Sends "ucinewgame" between positions for fair comparison
  - `false`: Reuses TT/history across positions (faster but less isolated)

**Validation:**
- [x] Configuration compiles without errors
- [x] New fields are optional (backward compatible)
- [x] Validation rejects missing engine files when enginePath is specified
- [x] TestSuiteRunner displays engine mode (internal/external)

**Example Configuration:**
```yaml
testSuites:
  # Internal engine (default)
  - name: "franky_tests"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000
    maxDepth: 30

  # External engine with position isolation
  - name: "franky_tests_v1.0"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    isolatePositions: true  # Clear state between positions (default)
```

**Next Steps:**
- Ready to proceed to Phase 2: UCIEngine implementation

---

### ✅ Phase 2: UCIEngine Class - Core Implementation (COMPLETE)
**Completed:** 2026-02-02
**Time:** ~3 hours

**Tasks Completed:**
- [x] Created `src/engine_arena/UCIEngine.h`
- [x] Created `src/engine_arena/UCIEngine.cpp`
- [x] Defined `UCISearchResult` struct with all required fields
- [x] Implemented subprocess management (Windows _popen, Linux popen)
- [x] Implemented UCI initialization (uci, isready)
- [x] Added configurable timeout mechanism (default 30s)
- [x] **Added `newGame()` method for position isolation**
- [x] Updated CMakeLists.txt to include UCIEngine.cpp

**Files Created:**
- Created: `src/engine_arena/UCIEngine.h`
- Created: `src/engine_arena/UCIEngine.cpp`

**Files Modified:**
- Modified: `src/CMakeLists.txt`

**Implementation Details:**

**UCISearchResult Structure:**
```cpp
struct UCISearchResult {
  std::string bestMove;       // UCI long algebraic, empty on error
  uint64_t nodes = 0;         // Total nodes searched
  Depth depth = DEPTH_ZERO;   // Search depth reached
  Value score = VALUE_NONE;   // Centipawn score
  milliseconds time{0};       // Time spent searching
};
```

**Subprocess Management:**
- Windows: Uses `_popen()` with cmd.exe wrapper
- Linux: Uses `popen()` directly
- Proper error handling and cleanup in destructor

**UCI Protocol Implementation:**
- Constructor: Starts engine, sends "uci", waits for "uciok"
- Captures engine name from "id name" response
- `newGame()`: Sends "ucinewgame" to clear TT/history between positions
- Sends "isready" / "readyok" for synchronization
- Destructor: Sends "quit" and closes process

**Engine Reuse Strategy:**
- **One UCIEngine instance per test suite** (not per position)
- `newGame()` called between positions to ensure fair comparison
- Avoids expensive process creation overhead
- Matches real-world UCI GUI usage pattern

**Timeout Handling:**
- Configurable via `setSearchTimeout()`
- Default: 30 seconds absolute timeout
- Uses platform-specific non-blocking I/O:
  - Windows: fgetc() with sleep polling
  - Linux: select() for proper timeout

**Error Handling:**
- Constructor throws on engine not found or UCI init failure
- search() returns empty bestMove on timeout/error
- All errors logged to stderr

**Validation:**
- [x] Code compiles without errors
- [x] Both Windows and Linux implementations included
- [x] `newGame()` method implemented
- [x] Manual testing with FrankyCPP v1.1 (successful - 2026-02-03)
- [x] **Refactored to Boost.Process v1 (2026-02-03)**

**Boost.Process Refactoring (2026-02-03):**
- [x] Replaced Windows CreateProcess (~200 lines) with Boost.Process v1 (~100 lines)
- [x] 50% code reduction, fully cross-platform implementation
- [x] Added boost-process dependency to vcpkg.json
- [x] Added Boost::filesystem to CMake (required by boost-process)
- [x] Thread-based timeout handling for blocking I/O
- [x] Handles FrankyCPP logger output mixed with UCI responses
- [x] Uses boost::process::v1::child, opstream, ipstream
- [x] Detects 'uciok' and 'readyok' within lines (not exact match)
- [x] All manual tests pass successfully

**Next Steps:**
- Ready to proceed to Phase 4: Move comparison logic

**Note:** Phase 3 (Search Implementation) was completed together with Phase 2,
as the search functionality is integral to the UCIEngine class design.

---

### ✅ Phase 3: UCIEngine Class - Search Implementation (COMPLETE)
**Completed:** 2026-02-02 (combined with Phase 2)
**Time:** Included in Phase 2 estimate

**Tasks Completed:**
- [x] Implemented `setPosition(const std::string& fen)`
- [x] Implemented `search(milliseconds timeMs, Depth maxDepth)`
- [x] Parse "bestmove" response
- [x] Parse "info" lines during search (nodes, depth, score, time)
- [x] Added timeout handling
- [x] Added error logging

**Implementation Details:**

**setPosition():**
- Sends "position fen <fen_string>"
- Waits for "readyok" to confirm ready

**search():**
- Builds "go" command with movetime and depth parameters
- Reads engine output line-by-line
- Parses "info" lines for statistics
- Extracts bestmove from "bestmove <move>" response
- Returns empty result on timeout/error

**Info Line Parsing:**
- Extracts: depth, nodes, time, score
- Handles both "score cp" (centipawns) and "score mate" (mate in N)
- Converts mate scores to Value representation

**Error Handling:**
- Timeout protection on all I/O operations
- Returns empty bestMove string on any failure
- Detailed error messages to stderr

**Validation:**
- [x] Code compiles without errors
- [x] Manual testing with real positions (successful - 2026-02-03)
- [x] Test 1 (Starting position): Returns valid move
- [x] Test 2 (Tactical position h5f7): Correctly finds best move
- [x] Engine reuse with newGame() works correctly

**UCI newGame Fix (2026-02-02):**
- [x] Fixed `UciHandler::uciNewGameCommand()` to call `Search::newGame()` instead of just `clearTT()`
- [x] Now properly clears: TT + History heuristics + PawnTT (via Evaluator recreation)
- [x] Ensures complete position isolation for external engine testing

**Result Structure Enhancement (2026-02-02):**
- [x] Added `engineName` and `enginePath` fields to `TestSuiteResult`
- [x] Updated JSON serialization/deserialization in `ResultWriter` and `ArenaRunner`
- [x] Enhanced comparison reports to display engine names
- [x] Old JSON files without engine info must be regenerated (no backward compatibility needed)

---

### ✅ Phase 4: Move Comparison Logic (COMPLETE)
**Completed:** 2026-02-03
**Time:** ~1 hour

**Tasks Completed:**
- [x] Created `src/chesscore/MoveUtils.h`
- [x] Created `src/chesscore/MoveUtils.cpp`
- [x] Created `test/chesscore/MoveUtils_Test.cpp`
- [x] Implemented `matchesExpectedMove()` function
- [x] Implemented `normalizeMove()` helper function
- [x] Support for long algebraic notation (direct string match)
- [x] Support for SAN notation (via conversion using MoveGenerator)
- [x] Handles multiple expected moves (for BM tests)
- [x] Comprehensive unit tests covering all notation types
- [x] Updated CMakeLists.txt to include new files
- [x] **Refactored to `chesscore/` for better architecture**

**Files Created:**
- Created: `src/chesscore/MoveUtils.h` (moved from `engine_arena/MoveComparison.h`)
- Created: `src/chesscore/MoveUtils.cpp` (moved from `engine_arena/MoveComparison.cpp`)
- Created: `test/chesscore/MoveUtils_Test.cpp` (moved from `engine_arena/MoveComparison_Test.cpp`)

**Files Modified:**
- Modified: `src/CMakeLists.txt` (added MoveUtils.cpp to FrankyCPPlib_SRCS)
- Modified: `test/CMakeLists.txt` (added MoveUtils_Test.cpp to test sources)

**Architectural Decision:**
- **Initial location:** `engine_arena/MoveComparison`
- **Final location:** `chesscore/MoveUtils`
- **Rationale:** 
  - MoveUtils depends on `chesscore/` classes (MoveGenerator, Position)
  - Keeps all dependencies in same architectural layer
  - More reusable - can be used by TestSuite, opening book, etc.
  - Better than `common/` which should have minimal dependencies
  - Cleaner dependency graph: stays within chesscore layer

**Implementation Details:**

**Location: `chesscore/MoveUtils`**
- Functions are at global scope (no namespace)
- Clean integration with other chesscore components
- Can be used by any module needing move comparison

**matchesExpectedMove() Strategy:**
1. **Fast Path:** Direct normalized string comparison
   - Handles UCI long algebraic format (e.g., "e2e4")
   - Case-insensitive comparison
   - Removes decoration characters (+, #, !, ?)

2. **Slow Path:** SAN conversion using existing code
   - Uses `MoveGenerator::getMoveFromUci()` for UCI parsing
   - Uses `MoveGenerator::getMoveFromSan()` for SAN parsing
   - Converts to long algebraic for comparison
   - Reuses battle-tested internal move parsing

**normalizeMove() Function:**
- Converts to lowercase
- Removes decoration characters (+, #, !, ?)
- Preserves equals sign for promotion notation (e.g., "e8=q")

**Supported Notations:**
- ✅ UCI Long Algebraic: "e2e4", "g1f3", "e1g1", "e7e8q"
- ✅ SAN Pawn Moves: "e4", "d5"
- ✅ SAN Piece Moves: "Nf3", "Bc4", "Qh5"
- ✅ SAN Castling: "O-O", "O-O-O"
- ✅ SAN Captures: "exd5", "Nxe5", "Qxf7+"
- ✅ SAN Promotions: "e8=Q", "e8Q", "a1=N"
- ✅ SAN Disambiguated: "Nbd2", "R1a3", "Qh4e1"

**Unit Test Coverage:**
- ✅ Direct long algebraic matching (case-insensitive)
- ✅ Decoration removal (+, #, !, ?)
- ✅ SAN pawn moves
- ✅ SAN piece moves (N, B, R, Q, K)
- ✅ SAN castling (both sides)
- ✅ SAN captures
- ✅ SAN promotions (multiple notations)
- ✅ SAN ambiguous notation
- ✅ Multiple expected moves
- ✅ Empty/invalid inputs
- ✅ Real-world EPD examples

**Validation:**
- [x] Code compiles without errors
- [x] No IDE errors or warnings
- [x] Comprehensive unit tests (29 test cases)
- [x] Tests cover all supported notation types
- [x] Tests cover edge cases and error conditions
- [x] Ready for integration in TestSuiteRunner

**Design Benefits:**
- ✅ Separate utility for better testability
- ✅ Reuses existing MoveGenerator code (no duplication)
- ✅ Clean separation of concerns
- ✅ Easy to unit test independently
- ✅ Maximum EPD compatibility

**Next Steps:**
- Ready to proceed to Phase 5: TestSuiteRunner Integration

---

### ⏳ Phase 5: TestSuiteRunner Implementation (PENDING)
**Status:** Not started
**Estimated Time:** 2-3 hours

---

### ⏳ Phase 6: Result Structure Updates (PENDING)
**Status:** Not started
**Estimated Time:** 1 hour

---

### ⏳ Phase 7: Testing & Error Handling (PENDING)
**Status:** Not started
**Estimated Time:** 2-3 hours

---

### ⏳ Phase 8: Documentation (PENDING)
**Status:** Not started
**Estimated Time:** 1 hour

---

## Implementation Notes

### Phase 1 Implementation Details

**Configuration Design:**
- `enginePath` is an optional field in `TestSuiteConfig`
- Empty `enginePath` means use internal engine (default)
- Non-empty `enginePath` triggers external UCI engine mode
- Validation ensures external engine executable exists

**Backward Compatibility:**
- All existing configurations work without modification
- No changes to TestSuite internal engine behavior
- External engine support is purely additive

**Example Configuration:**
```yaml
testSuites:
  # Internal engine (default)
  - name: "franky_tests"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000
    maxDepth: 30

  # External engine (new)
  - name: "franky_tests_v1.0"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
```

---

## Total Progress

**Estimated Total Time:** 14-18 hours
**Time Spent:** 11 hours (Phase 0: 3h, Phase 1: 0.5h, Phases 2+3: 6.5h, Phase 4: 1h)
**Time Remaining:** 3-7 hours

**Phase Completion:**
- Phase 0: ✅ Complete
- Phase 1: ✅ Complete
- Phase 2: ✅ Complete (includes Boost.Process refactoring)
- Phase 3: ✅ Complete
- Phase 4: ✅ Complete
- Phases 5-8: ⏳ Pending

**Overall Progress:** 5/9 phases complete (56%)

---

## Next Actions

1. **Start Phase 5:** Integrate UCIEngine and MoveUtils into TestSuiteRunner
2. **Priority:** Implement external engine test execution loop
3. **Testing:** Validate with franky_tests.epd against v1.0 engine
4. **Integration:** Connect all components for end-to-end testing

---

*Last updated: 2026-02-03*
