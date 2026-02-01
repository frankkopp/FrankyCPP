# External Engine Test Suite Support - Implementation Outline

## Status: DESIGN PHASE (No Changes Made)

## Date: 2026-02-01

---

## Problem Statement

**Current Limitation:**
- `TestSuiteRunner` only runs test suites on the **built-in engine** (the engine it's compiled with)
- Cannot test older versions like v0.5, v1.0 against current test suites
- Cannot compare tactical strength improvements across versions using EPD tests
- `MatchRunner` supports external engines, but `TestSuiteRunner` doesn't

**Desired Capability:**
Run EPD test suites (WAC, STS, etc.) against:
1. Current built-in engine (existing behavior)
2. External older engine executables (v0.5, v1.0, etc.) via UCI
3. Any UCI-compatible chess engine for comparison

---

## Current Architecture

### How TestSuiteRunner Currently Works

```
TestSuiteRunner
    ↓
  TestSuite (enginetest/TestSuite.h)
    ↓
  Search (engine/Search.h) - BUILT-IN ENGINE
    ↓
  Results → JSON with version tag
```

**Key Issue:** `TestSuite` class directly instantiates and uses the built-in `Search` engine. There's no abstraction layer for external engines.

### How MatchRunner Works (for comparison)

```
MatchRunner
    ↓
  cutechess-cli subprocess
    ↓
  Engine1.exe ← UCI protocol → Engine2.exe (EXTERNAL)
    ↓
  Parse output → Results → JSON
```

**Key Difference:** `MatchRunner` uses `cutechess-cli` which handles UCI communication with external engines.

---

## Design Options

### Option 1: Add UCI Wrapper to TestSuiteRunner (Recommended)

**Approach:** Create a UCI adapter that can communicate with external engines, mimicking the internal Search interface.

**Architecture:**
```
TestSuiteRunner
    ↓
  [MODE SELECTION]
    ↓
  Internal Mode: TestSuite → Search (built-in)
  External Mode: ExternalEngineAdapter → UCI subprocess
    ↓
  Results → JSON with version tag
```

**Pros:**
- Clean separation between internal and external testing
- Reuses existing TestSuite logic for internal engine
- Similar pattern to MatchRunner (subprocess + UCI)
- No changes to existing TestSuite/Search classes
- Can test ANY UCI engine, not just FrankyCPP versions

**Cons:**
- More complex implementation (UCI protocol handling)
- Need subprocess management and I/O parsing
- Slower than internal testing (UCI overhead)

---

### Option 2: Use cutechess-cli for Test Suites

**Approach:** Convert EPD test suites to run through cutechess-cli like matches.

**Architecture:**
```
TestSuiteRunner
    ↓
  cutechess-cli with EPD positions
    ↓
  External engine via UCI
    ↓
  Parse cutechess output → Results
```

**Pros:**
- Reuses cutechess-cli infrastructure
- Leverages proven UCI communication
- Simpler implementation (similar to MatchRunner)

**Cons:**
- cutechess-cli not designed for tactical test suites
- Less control over position-by-position testing
- May not support all EPD test types (dm, bm, am)
- More complex result parsing

---

### Option 3: Modify TestSuite to Accept External Search

**Approach:** Refactor TestSuite to use dependency injection for the search engine.

**Architecture:**
```
TestSuiteRunner
    ↓
  TestSuite (modified to accept ISearchEngine)
    ↓
  Internal: SearchAdapter → Search (built-in)
  External: UCISearchAdapter → UCI subprocess
    ↓
  Results → JSON
```

**Pros:**
- Most flexible architecture
- Single test suite execution path
- Future-proof for other engine types

**Cons:**
- Requires significant refactoring of TestSuite
- Changes to core engine test infrastructure
- May break existing tests
- Higher implementation complexity

---

## Recommended Approach: Option 1 (UCI Adapter)

### Why Option 1?

1. **Non-invasive:** No changes to existing TestSuite or Search classes
2. **Similar to MatchRunner:** Reuses subprocess + UCI communication pattern
3. **Flexible:** Can test any UCI engine, not just FrankyCPP
4. **Maintainable:** Clear separation between internal and external modes
5. **Proven pattern:** UCI communication is well-understood

---

## Implementation Plan (Option 1)

### Phase 1: Configuration Changes

**File:** `src/engine_arena/ArenaConfig.h`

**Add to TestSuiteConfig struct:**
```cpp
struct TestSuiteConfig {
  std::string name;
  std::string epdPath;
  milliseconds timePerMove;
  Depth maxDepth;
  
  // NEW FIELDS:
  std::string enginePath = "";  // Empty = use built-in engine
  bool useExternalEngine = false;  // Flag for mode selection
};
```

**YAML format:**
```yaml
testSuites:
  # Internal engine (existing behavior)
  - name: "v1.1_WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
    # No enginePath = uses built-in engine

  # External engine (new capability)
  - name: "v0.5_WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V0.5/FrankyCPP_v0.5.exe"
    useExternalEngine: true
```

**Changes Required:**
- Add 2 new fields to `TestSuiteConfig` struct
- Update YAML parsing in `ArenaConfig.cpp` to read new fields
- Update validation to check `enginePath` exists if `useExternalEngine` is true

---

### Phase 2: UCI Adapter Implementation

**New Files:**
- `src/engine_arena/UCIEngine.h` - UCI communication interface
- `src/engine_arena/UCIEngine.cpp` - UCI subprocess + I/O handling

**UCIEngine Class Responsibilities:**
```cpp
class UCIEngine {
public:
  // Constructor - starts engine subprocess
  explicit UCIEngine(const std::string& enginePath);
  
  // Destructor - stops engine cleanly
  ~UCIEngine();
  
  // Initialize engine (uci, isready)
  void initialize();
  
  // Set position from FEN
  void setPosition(const std::string& fen);
  
  // Search with time limit, return best move
  Move search(milliseconds timeMs, Depth maxDepth);
  
  // Check if engine supports required UCI options
  bool isReady() const;
  
private:
  std::string enginePath;
  // Subprocess handle (platform-specific)
  // Input/output pipes for UCI communication
  // UCI option state
};
```

**Key Methods:**
1. **initialize():** Send "uci", wait for "uciok", send "isready", wait for "readyok"
2. **setPosition():** Send "position fen <fen_string>"
3. **search():** Send "go movetime <ms> depth <depth>", parse "bestmove" response
4. **Process I/O:** Non-blocking pipe reading, line buffering, timeout handling

**Platform Considerations:**
- Windows: Use `CreateProcess` + pipes (similar to MatchRunner's `_popen`)
- Linux: Use `popen` or `fork/exec` with pipes
- Timeout handling: Need to kill engine if it hangs

---

### Phase 3: TestSuiteRunner Modifications

**File:** `src/engine_arena/TestSuiteRunner.cpp`

**Modified runTestSuite() method:**
```cpp
TestSuiteResult TestSuiteRunner::runTestSuite(const TestSuiteConfig& suiteConfig) {
  // Check configuration
  if (suiteConfig.useExternalEngine) {
    // NEW: External engine mode
    return runTestSuiteExternal(suiteConfig);
  } else {
    // EXISTING: Internal engine mode
    return runTestSuiteInternal(suiteConfig);
  }
}

// EXISTING CODE (renamed)
TestSuiteResult TestSuiteRunner::runTestSuiteInternal(const TestSuiteConfig& suiteConfig) {
  // Current implementation - uses TestSuite class directly
  TestSuite suite(suiteConfig.timePerMove, suiteConfig.maxDepth, suiteConfig.epdPath);
  suite.runTestSuite();
  // ... convert and return results
}

// NEW METHOD
TestSuiteResult TestSuiteRunner::runTestSuiteExternal(const TestSuiteConfig& suiteConfig) {
  // 1. Load EPD file manually (parse FEN + test conditions)
  // 2. Create UCIEngine instance
  // 3. For each position:
  //    - engine.setPosition(fen)
  //    - bestMove = engine.search(timeMs, depth)
  //    - Compare with expected move(s)
  //    - Capture results
  // 4. Convert to TestSuiteResult
  // 5. Return results
}
```

**EPD Parsing:**
- Need to parse EPD format manually (currently done inside TestSuite)
- Extract: FEN, operation (bm/am/dm), expected moves, test ID
- Can reuse/expose TestSuite's static parsing methods

---

### Phase 4: EPD Parser Extraction

**Option A: Expose TestSuite parsing methods**
```cpp
// In TestSuite.h - make these public or static
public:
  static std::vector<Test> parseEPDFile(const std::string& filePath);
  static bool parseEPDLine(const std::string& line, Test& test);
```

**Option B: Create separate EPD parser**
```cpp
// New file: src/engine_arena/EPDParser.h
class EPDParser {
public:
  static std::vector<EPDTest> parseFile(const std::string& path);
  
private:
  static bool parseLine(const std::string& line, EPDTest& test);
};

struct EPDTest {
  std::string fen;
  std::string operation;  // "bm", "am", "dm"
  std::vector<std::string> expectedMoves;
  std::string testId;
};
```

**Recommendation:** Option B - keeps arena code independent of enginetest

---

### Phase 5: Result Comparison Logic

**Challenge:** External engine returns move string (e.g., "e2e4"), need to compare with EPD expected moves.

**Solution:**
```cpp
bool TestSuiteRunner::checkMove(const Move& actualMove, 
                                 const std::vector<std::string>& expectedMoves,
                                 const Position& position) {
  std::string actualMoveStr = actualMove.str();  // "e2e4"
  
  for (const auto& expected : expectedMoves) {
    if (actualMoveStr == expected) return true;
    // Also try SAN notation matching if needed
  }
  
  return false;
}
```

---

## File Changes Summary

### New Files
1. **`src/engine_arena/UCIEngine.h`** - UCI communication interface (150 lines)
2. **`src/engine_arena/UCIEngine.cpp`** - UCI subprocess implementation (300 lines)
3. **`src/engine_arena/EPDParser.h`** - EPD file parsing (100 lines)
4. **`src/engine_arena/EPDParser.cpp`** - EPD parsing implementation (200 lines)

### Modified Files
1. **`src/engine_arena/ArenaConfig.h`** - Add 2 fields to TestSuiteConfig
2. **`src/engine_arena/ArenaConfig.cpp`** - Parse new YAML fields
3. **`src/engine_arena/TestSuiteRunner.h`** - Add runTestSuiteExternal() method
4. **`src/engine_arena/TestSuiteRunner.cpp`** - Implement external engine mode (200 lines)
5. **`src/CMakeLists.txt`** - Add UCIEngine.cpp and EPDParser.cpp to build
6. **`config/arena.yaml`** - Add example external engine test suites

### No Changes Required
- `TestSuite.h/.cpp` - Unchanged (used only for internal engine)
- `Search.h/.cpp` - Unchanged
- `MatchRunner` - Unchanged (already supports external engines)

---

## Estimated Effort

| Component | Lines of Code | Time Estimate |
|-----------|---------------|---------------|
| Configuration changes | ~50 | 30 min |
| UCIEngine implementation | ~450 | 3-4 hours |
| EPDParser implementation | ~300 | 2-3 hours |
| TestSuiteRunner modifications | ~250 | 2-3 hours |
| Testing & debugging | N/A | 2-3 hours |
| Documentation | ~200 | 1 hour |
| **TOTAL** | **~1250 lines** | **10-14 hours** |

---

## Testing Strategy

### Unit Tests
1. **UCIEngine tests:**
   - Start/stop engine subprocess
   - Send UCI commands, parse responses
   - Timeout handling
   - Error cases (engine not found, crashes, invalid moves)

2. **EPDParser tests:**
   - Parse valid EPD lines
   - Handle malformed EPD
   - Extract operations (bm, am, dm)
   - Parse move lists

### Integration Tests
1. Run simple EPD test against external v0.5 engine
2. Compare results with internal engine on same positions
3. Verify JSON output format is identical
4. Test error handling (missing engine, invalid paths)

### End-to-End Tests
1. Run full WAC suite against v0.5 external engine
2. Run full WAC suite against v1.1 internal engine
3. Compare results using `--compare v1.1 v0.5`
4. Verify comparison report shows differences

---

## Configuration Examples

### arena.yaml with External Engines

```yaml
version: "v1.1"  # Version being tested currently
resultsDir: "./results"

testSuites:
  # Test current built-in engine
  - name: "v1.1_WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
    # No enginePath = uses built-in engine

  # Test v1.0 external engine
  - name: "v1.0_WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    useExternalEngine: true

  # Test v0.5 external engine
  - name: "v0.5_WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V0.5/FrankyCPP_v0.5.exe"
    useExternalEngine: true
```

### Usage Workflow

```powershell
# Run all test suites (internal v1.1 + external v1.0 + external v0.5)
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --testsuites

# Results saved:
# - results/testsuites/v1.1_v1.1_WAC_20260201_143000.json
# - results/testsuites/v1.0_v1.0_WAC_20260201_143200.json
# - results/testsuites/v0.5_v0.5_WAC_20260201_143400.json

# Compare tactical strength across versions
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --compare v1.1 v1.0
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --compare v1.1 v0.5
```

---

## Alternative: Simpler Approach

If full UCI implementation is too complex, consider a **hybrid approach:**

1. **Keep internal engine testing as-is** (fast, detailed)
2. **Use MatchRunner pattern for external engines:**
   - Create a "tactical test runner" using cutechess-cli's EPD support
   - Parse cutechess output for test results
   - Simpler but less flexible

This would be ~200 lines of code instead of ~1250, but with less control.

---

## Risks & Considerations

### Technical Risks
1. **UCI protocol complexity:** Need to handle all edge cases (timeouts, invalid moves, crashes)
2. **Platform differences:** Windows/Linux subprocess handling differs
3. **Performance:** External engines slower than internal (UCI overhead)
4. **Move notation:** Need to handle both long algebraic (e2e4) and SAN (Nf3)

### Design Risks
1. **Code duplication:** Two paths for test suite execution (internal vs external)
2. **Maintenance:** Need to keep both paths working
3. **Testing complexity:** Harder to test external engine path

### Mitigation
1. Share as much code as possible between internal/external paths
2. Comprehensive error handling and logging
3. Clear documentation on when to use each mode
4. Automated tests for UCI communication layer

---

## Decision Required

**Before proceeding with implementation, decide:**

1. **Is Option 1 (UCI Adapter) the right approach?**
   - Alternatives: Option 2 (cutechess-cli) or Option 3 (refactor TestSuite)

2. **Is 10-14 hours of work justified for this feature?**
   - Benefit: Compare tactical strength across all versions
   - Cost: ~1250 lines of code, UCI complexity

3. **Should we implement this now or defer to later?**
   - Phase 4 (comparison) already complete and working for matches
   - Could use matches for version comparison, test suites for internal testing only

---

## Summary

**Current State:** Test suites only work with built-in engine

**Proposed Solution:** Add UCI adapter to TestSuiteRunner for external engines

**Effort:** 10-14 hours, ~1250 lines of code

**Benefit:** Compare tactical strength (WAC, STS, etc.) across all FrankyCPP versions

**Risk:** Moderate complexity (UCI protocol, subprocess management)

**Recommendation:** Implement if historical tactical strength comparison is important. Otherwise, use matches for version comparison and keep test suites for internal development only.

---

*Design document created: 2026-02-01*
*Status: Awaiting approval to proceed with implementation*
