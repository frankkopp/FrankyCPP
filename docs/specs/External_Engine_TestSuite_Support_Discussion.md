# External Engine Test Suite Support - Design Discussion

## Status: ✅ APPROVED - READY FOR IMPLEMENTATION

## Date: 2026-02-01
## Updated: 2026-02-01
## Approved: 2026-02-01

## Purpose

This document contains discussion points and open questions for refining the Option 1 (UCI Adapter) implementation plan. Work through each section to make decisions before implementation begins.

**STATUS:** All decisions made, phases reviewed and approved. Implementation will begin in a new branch after committing this document.

**Related Documents:**
- `External_Engine_TestSuite_Support_Outline.md` - Main implementation plan
- `docs/analysis/cutechess_epd_capabilities.md` - Why Option 2 was ruled out

---

## 📋 Discussion Checklist

- [x] 1. Architecture & Code Reuse - **DECIDED**
- [x] 2. EPD Parsing Strategy - **DECIDED**
- [x] 3. UCIEngine Interface Design - **DECIDED**
- [x] 4. Configuration Changes - **DECIDED**
- [x] 5. Move Comparison Logic - **DECIDED**
- [x] 6. Result Structure Compatibility - **DECIDED**
- [x] 7. Testing Strategy - **DECIDED**
- [x] 8. Implementation Phases - **APPROVED ✅**
- [x] 9. Risk Assessment - **REVIEWED**
- [x] 10. Final Decisions Summary - **COMPLETE**

---

## 1️⃣ Architecture & Code Reuse ✅ DECIDED

### Decisions Made

#### 1.1 UCIEngine Class Location: **A) engine_arena**
- Location: `src/engine_arena/UCIEngine.{h,cpp}`
- Rationale: Keep arena features together, can refactor later if needed

#### 1.2 Subprocess Management: **B) Duplicate**
- Duplicate subprocess logic in UCIEngine
- Rationale: Lower risk, no changes to MatchRunner, get it working first


---

## 2️⃣ EPD Parsing Strategy ✅ DECIDED

### Decision Made

#### 2.1 EPD Parser Approach: **C) Extract to Shared Utility**

**Implementation:**
- New file: `src/common/EPDParser.h/.cpp`
- Shared by both `TestSuite` and `TestSuiteRunner`
- Refactor `TestSuite` to use shared parser

**Rationale:**
- Single parser implementation (DRY principle)
- Proper separation of concerns
- Best long-term design

**Important:**
- **Must be done as separate step**
- Test `TestSuite` after each change to ensure it still works
- Higher priority on maintaining existing functionality

**Implementation Notes:**
- Extract parsing logic from `TestSuite::readTestCases()` and `TestSuite::readOneEPD()`
- Create platform-independent EPD data structure
- Update `TestSuite` to use new parser
- Verify all existing tests still pass before proceeding to external engine support

---

## 3️⃣ UCIEngine Interface Design ✅ DECIDED

### Decisions Made

#### 3.1 Search Method Return Type: **A) Return std::string**

**Implementation:**
```cpp
struct UCISearchResult {
  std::string bestMove;       // UCI long algebraic (e.g., "e2e4")
  uint64_t nodes = 0;
  Depth depth = 0;
  Value score = VALUE_NONE;   // Parsed from info lines
  milliseconds time{0};
};

class UCIEngine {
public:
  UCISearchResult search(milliseconds timeMs, Depth maxDepth);
};
```

**Rationale:** Cleaner separation of concerns, no coupling to internal Move type

---

#### 3.2 Search Info Capture: **A) Capture Stats Including Score**

**Stats to Capture:**
- ✅ Nodes
- ✅ Depth
- ✅ Time
- ✅ Score (required for DM mode mate-in-N detection)
- ✅ NPS (calculated from nodes/time)

**Rationale:** 
- Required for DM (direct mate) tests - need to verify mate score
- Consistent with internal engine results
- Useful for performance comparison

---

#### 3.3 Error Handling Strategy: **Agreed with Configurable Timeout**

**Implementation:**
```cpp
class UCIEngine {
public:
  explicit UCIEngine(const std::string& enginePath);
  
  UCISearchResult search(milliseconds timeMs, Depth maxDepth);
  
  // Timeout: configurable absolute value (not relative to search time)
  void setSearchTimeout(milliseconds absoluteTimeout);
  
private:
  milliseconds searchTimeout = milliseconds{30000};  // Default 30 seconds
};
```

**Error Scenarios:**
1. Constructor throws if engine not found or won't start
2. search() returns empty bestMove on error, logs details
3. Timeout aborts search if no response within absolute timeout
4. Invalid move logged, marked as FAILED, continues suite
5. Malformed UCI output logged, best-effort parsing

**Timeout:** Configurable absolute value (not relative to search time)

---

## 4️⃣ Configuration Changes ✅ DECIDED

### Decisions Made

#### 4.1 External Engine Configuration: **External-Only Mode**

**Decision:** Arena only supports external engines - `enginePath` is **always required**

**Implementation:**
```cpp
struct TestSuiteConfig {
  std::string name;
  std::string epdPath;
  milliseconds timePerMove;
  Depth maxDepth;
  
  // NEW:
  std::string enginePath;  // REQUIRED - no default, always external
};
```

**Rationale:** 
- TestSuite already tests built-in engine via direct API
- Arena is for version comparison - always external
- Simpler code - single execution path
- Consistent interface - all engines tested identically

**Impact:**
- No mode switching logic needed
- No `if (enginePath.empty())` checks
- TestSuiteRunner only needs `runTestSuiteExternal()` method
- Clearer separation: TestSuite = internal, Arena = external

---

#### 4.2 Per-Suite Time/Depth Customization: **NO - Defeats Purpose**

**Decision:** Each suite must use same time/depth limits for fair comparison

**Rationale:**
- Different time limits defeat the purpose of comparison
- Lower limits also mean weaker performance - not a fair test
- All engines tested under identical conditions

**YAML Example:**
```yaml
testSuites:
  - name: "v1.1_WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30

  - name: "v1.0_WAC"
    epdPath: "test/testsets/wac.epd"
    enginePath: "Release/v1.0/FrankyCPP_v1.0.exe"
    timePerMove: 5000   # SAME limits for fair comparison
    maxDepth: 30

  - name: "v0.5_WAC"
    epdPath: "test/testsets/wac.epd"
    enginePath: "Release/v0.5/FrankyCPP_v0.5.exe"
    timePerMove: 5000   # SAME limits
    maxDepth: 30
```

---

#### 4.3 UCI Options Support: **DEFERRED**

**Decision:** Defer to future version

**Rationale:**
- Each engine typically has its own config file anyway
- Would complicate UCI handling significantly
- Not needed for v1
- Can add later if needed

---

## 5️⃣ Move Comparison Logic ✅ DECIDED

### Decisions Made

#### 5.1 Move Format Support: **B) Support Both Long Algebraic and SAN**

**Implementation:**
```cpp
bool matchesExpectedMove(const std::string& actualMove, 
                         const std::vector<std::string>& expectedMoves,
                         const Position& position) {
  // 1. Try direct match (long algebraic)
  for (const auto& expected : expectedMoves) {
    if (actualMove == expected) return true;
  }
  
  // 2. Try SAN conversion
  // Convert expected SAN to long algebraic using position
  // Compare with actualMove
  
  return false;
}
```

**Rationale:** Maximum EPD compatibility, works with any format

**Code Reuse Options:**
- Potentially reuse from engine code (Move/Position classes)
- Or from opening book SAN parsing

---

#### 5.2 SAN Conversion Implementation: **A) Reuse Internal Code**

**Approach:**
- Use existing `Position::moveFromString()` or similar
- Leverage internal move generation
- Don't reinvent the wheel

**Rationale:** Engine already has working SAN parser, no need to duplicate

---

## 6️⃣ Result Structure Compatibility ✅ DECIDED

### Decisions Made

#### 6.1 Result Structure: **YES - Same Structure**

**Decision:** External engine results use same `TestSuiteResult` structure

**Rationale:** Allows reuse of ResultWriter and comparison logic

---

#### 6.2 External Engine Metadata: **Add Metadata Fields**

**Implementation:**
```cpp
struct TestSuiteResult {
  // ...existing fields...
  
  // NEW:
  std::string enginePath;          // Path to engine executable (always present)
  std::string engineVersion = "";  // From UCI "id name"
};
```

**Fields to Add:**
- ✅ `enginePath` (std::string) - Path to engine executable
- ✅ `engineVersion` (std::string) - from UCI "id name"
- ❌ ~~`isExternalEngine`~~ - NOT needed (always external in Arena)

**Rationale:** 
- Arena only uses external engines
- enginePath identifies which engine was tested
- engineVersion from UCI for verification

---

## 7️⃣ Testing Strategy ✅ DECIDED

### Decisions Made

#### 7.1 Unit Testing: **A) Real Engine Tests**

**Approach:** Test UCIEngine against actual FrankyCPP executables

**Rationale:** Most practical, already have engines built, realistic testing

---

#### 7.2 Integration Testing Scope: **AGREED**

**Test Cases:**
- ✅ Small EPD suite (5-10 positions) against v1.0 engine
- ✅ Same suite against built-in engine (compare results)
- ✅ Missing engine executable (error handling)
- ✅ Invalid EPD file (parser error handling)
- ✅ Engine crash during search (timeout/recovery)
- ✅ Multiple suites in sequence
- ✅ JSON output format validation

---

#### 7.3 Test EPD Selection: **test/testsets/franky_tests.epd**

**Decision:** Use existing `test/testsets/franky_tests.epd`

**Rationale:** Already has the purpose of a minimal test set

---

## 8️⃣ Implementation Phases (Revised) ✅ APPROVED

### Implementation Strategy

Based on decisions made, implementation will proceed in carefully sequenced phases with emphasis on maintaining existing functionality.

**STATUS:** All phases reviewed and approved for implementation.

---

### COMPLETE: **Phase 0: EPD Parser Extraction** (CRITICAL - Separate Step)
**Effort:** 3-4 hours  
**Priority:** HIGH - Must be done first and tested thoroughly

**Tasks:**
- [ ] Create `src/common/EPDParser.h`
- [ ] Create `src/common/EPDParser.cpp`
- [ ] Define shared EPD data structure:
  ```cpp
  struct EPDTest {
    std::string fen;
    TestType type;  // BM, AM, DM
    std::vector<std::string> expectedMoves;
    int mateDepth;
    std::string testId;
    std::string rawLine;
  };
  ```
- [ ] Extract parsing logic from `TestSuite::readTestCases()`
- [ ] Extract parsing logic from `TestSuite::readOneEPD()`
- [ ] Implement `EPDParser::parseFile()` and `EPDParser::parseLine()`
- [ ] **CRITICAL:** Refactor `TestSuite` to use new parser
- [ ] **CRITICAL:** Run ALL existing test suites to verify no regression
- [ ] Fix any issues before proceeding to next phase

**Validation:**
- All existing tests pass with identical results
- No changes to test suite behavior
- Parser handles all test formats (BM, AM, DM)

**Why Separate:** This refactoring touches working code and must be validated independently

---

### **Phase 1: Configuration** 
**Effort:** 30 min - 1 hour

**Tasks:**
- [ ] Add `enginePath` field to `TestSuiteConfig` struct in `ArenaConfig.h`
- [ ] Update YAML parsing in `ArenaConfig.cpp` to read `enginePath`
- [ ] Add validation: check `enginePath` exists if non-empty
- [ ] Update `arena.yaml` with example external engine configuration

**Validation:**
- Load config with external engine paths
- Validation rejects missing engine files

---

### **Phase 2: UCIEngine Class - Core Implementation**
**Effort:** 3-4 hours

**Tasks:**
- [ ] Create `src/engine_arena/UCIEngine.h`
- [ ] Create `src/engine_arena/UCIEngine.cpp`
- [ ] Define `UCISearchResult` struct:
  ```cpp
  struct UCISearchResult {
    std::string bestMove;
    uint64_t nodes = 0;
    Depth depth = 0;
    Value score = VALUE_NONE;
    milliseconds time{0};
  };
  ```
- [ ] Implement subprocess management (duplicate from MatchRunner):
  - Windows: `_popen()` with cmd.exe wrapper
  - Linux: `popen()` with direct command
- [ ] Implement UCI initialization:
  - Send "uci", wait for "uciok"
  - Parse "id name" for engine version
  - Send "isready", wait for "readyok"
- [ ] Implement configurable timeout mechanism
- [ ] Add CMakeLists.txt entry for UCIEngine.cpp

**Validation:**
- Can start/stop FrankyCPP executable
- Successful UCI handshake
- Captures engine version from "id name"

---

### **Phase 3: UCIEngine Class - Search Implementation**
**Effort:** 2-3 hours

**Tasks:**
- [ ] Implement `setPosition(const std::string& fen)`
  - Send "position fen <fen_string>"
- [ ] Implement `search(milliseconds timeMs, Depth maxDepth)`
  - Send "go movetime <ms> depth <depth>"
  - Parse "bestmove <move>" response
  - Return empty string on error/timeout
- [ ] Parse `info` lines during search:
  - Extract nodes
  - Extract depth
  - Extract score (required for DM tests)
  - Extract time
- [ ] Add timeout handling (configurable absolute value)
- [ ] Add error logging

**Validation:**
- Search returns correct best move from test positions
- Stats captured correctly (nodes, depth, score, time)
- Timeout triggers correctly
- Handles malformed output gracefully

---

### **Phase 4: Move Comparison Logic**
**Effort:** 1-2 hours

**Tasks:**
- [ ] Implement `matchesExpectedMove()` function
- [ ] Support long algebraic format (direct string match)
- [ ] Support SAN format conversion:
  - Use existing `Position::moveFromString()` or similar
  - Generate legal moves for position
  - Convert SAN to long algebraic
  - Compare with actual move
- [ ] Handle multiple expected moves (bm/am with multiple options)

**Validation:**
- Correctly matches long algebraic moves
- Correctly matches SAN moves
- Handles ambiguous SAN notation
- Handles castling notation (O-O, O-O-O)

---

### **Phase 5: TestSuiteRunner Implementation**
**Effort:** 2-3 hours

**Tasks:**
- [ ] Rename `runTestSuite()` to just run external mode (no routing)
- [ ] Implement test loop:
  ```cpp
  // Always external - no mode switching
  UCIEngine engine(config.enginePath);
  for (auto& test : epdTests) {
    Position pos{test.fen};
    UCISearchResult result = engine.search(timePerMove, maxDepth);
    bool passed = matchesExpectedMove(result.bestMove, test.expectedMoves, pos);
    // Store results...
  }
  ```
- [ ] Handle BM (best move) tests
- [ ] Handle AM (avoid move) tests
- [ ] Handle DM (direct mate) tests:
  - Check score for mate indication
  - Verify mate depth matches expected
- [ ] Aggregate results into `TestSuiteResult`
- [ ] No routing logic needed - always external

**Validation:**
- Run franky_tests.epd against v1.0 engine
- Run franky_tests.epd against v1.1 engine (current)
- Results match expected format
- All test types work (BM, AM, DM)

---

### **Phase 6: Result Structure Updates**
**Effort:** 1 hour

**Tasks:**
- [ ] Add metadata fields to `TestSuiteResult`:
  - `bool isExternalEngine`
  - `std::string enginePath`
  - `std::string engineVersion`
- [ ] Update ResultWriter to include new fields in JSON
- [ ] Update comparison tool to handle external engine metadata

**Validation:**
- JSON output includes external engine metadata
- Comparison tool distinguishes internal vs external

---

### **Phase 7: Testing & Error Handling**
**Effort:** 2-3 hours

**Tasks:**
- [ ] Integration test: franky_tests.epd against v1.0
- [ ] Integration test: Compare internal vs external results
- [ ] Error test: Missing engine executable
- [ ] Error test: Engine crashes during search
- [ ] Error test: Invalid EPD file
- [ ] Error test: Timeout handling
- [ ] Multiple suites in sequence
- [ ] Verify JSON output format
- [ ] Test comparison reports

**Validation:**
- All error cases handled gracefully
- Suite continues after position failure
- Proper error logging
- JSON format consistent

---

### **Phase 8: Documentation**
**Effort:** 1 hour

**Tasks:**
- [ ] Update `config/arena.yaml` with comprehensive examples
- [ ] Document external engine configuration
- [ ] Add code comments to UCIEngine class
- [ ] Update outline document with implementation notes
- [ ] Document timeout configuration
- [ ] Add troubleshooting guide

---

### **Total Estimated Time: 14-18 hours**
- Phase 0 (EPD Parser): 3-4 hours ⚠️ **CRITICAL PATH**
- Phases 1-8: 11-14 hours

---

### **Implementation Order**

**MUST follow this sequence:**

1. **Phase 0 FIRST** - EPD Parser extraction and validation
   - **STOP and validate thoroughly before proceeding**
   
2. **Phase 1** - Configuration (simple, low risk)

3. **Phases 2-3** - UCIEngine implementation (can test independently)

4. **Phase 4** - Move comparison (depends on Phase 3)

5. **Phase 5** - Integration (brings everything together)

6. **Phase 6** - Result structure (minor updates)

7. **Phase 7** - Testing (comprehensive validation)

8. **Phase 8** - Documentation (final step)

---

### **Critical Success Factors**

1. ✅ **Phase 0 must complete successfully first**
   - No shortcuts - TestSuite must work identically after refactor
   
2. ✅ **Test after each phase**
   - Don't proceed if current phase has issues
   
3. ✅ **Keep phases small and focused**
   - Easier to debug and validate
   
4. ✅ **Maintain existing functionality**
   - Never break working code

---

## 9️⃣ Risk Assessment

### Technical Risks

| Risk | Severity | Likelihood | Mitigation |
|------|----------|------------|------------|
| **Engine hangs/crashes** | High | Medium | Timeout mechanism (5-10s grace), abort position gracefully |
| **UCI protocol variations** | Medium | Medium | Handle common variations, log unexpected responses |
| **Move format mismatches** | Medium | High | Support both long algebraic and SAN, extensive testing |
| **Old engine bugs** | Low | Medium | Skip position on error, log and continue suite |
| **Platform differences** | Medium | Low | Test on both Windows and Linux before release |
| **Performance degradation** | Low | Low | External is inherently slower, acceptable for testing |
| **Memory leaks** | Medium | Low | Proper subprocess cleanup, RAII patterns |

### Design Risks

| Risk | Severity | Likelihood | Mitigation |
|------|----------|------------|------------|
| **Code duplication** | Low | High | Document reasons, plan future consolidation |
| **Maintenance burden** | Medium | Medium | Clear documentation, comprehensive tests |
| **API changes in TestSuite** | Low | Low | Not touching TestSuite internals |
| **Breaking existing tests** | High | Low | Not modifying existing code paths |

**Question:** Any additional risks to consider?

**Notes:**
```


```

---

## 🔟 Final Decisions Summary ✅ COMPLETE

### All Decisions Made - Ready for Implementation

| # | Decision Point | Chosen Option | Rationale |
|---|----------------|---------------|-----------|
| 1.1 | UCIEngine Location | **A) engine_arena** | Keep arena features together, can refactor later |
| 1.2 | Subprocess Management | **B) Duplicate** | Lower risk, no changes to MatchRunner |
| 2.1 | EPD Parser Approach | **C) Shared Utility** | Single implementation, proper separation, DRY principle |
| 3.1 | Search Return Type | **A) String (UCISearchResult)** | Cleaner separation, no coupling to Move type |
| 3.2 | Search Info Capture | **A) Capture stats + score** | Required for DM tests, consistent with internal |
| 3.3 | Error Handling | **Configurable timeout** | Absolute timeout value, graceful error handling |
| 4.1 | Config Flag | **No flag** | Just check enginePath.empty() |
| 4.2 | Per-Suite Limits | **NO - Same limits** | Fair comparison requires identical conditions |
| 4.3 | UCI Options | **DEFERRED** | Not needed for v1, adds complexity |
| 5.1 | Move Format Support | **B) Both LA and SAN** | Maximum EPD compatibility |
| 5.2 | SAN Conversion | **A) Reuse internal code** | Don't reinvent the wheel |
| 6.1 | Result Structure | **Same structure** | Reuse ResultWriter and comparison logic |
| 6.2 | External Metadata | **A) Add fields** | enginePath, engineVersion (no isExternalEngine - always external) |
| 7.1 | Unit Testing | **A) Real engine** | Most practical, realistic testing |
| 7.2 | Integration Tests | **Agreed** | Comprehensive test coverage |
| 7.3 | Test EPD | **franky_tests.epd** | Already exists for minimal testing |

---

### Resolved Questions

1. **Built-in vs External Mode: ✅ DECIDED - External-Only**
   
   **Decision:** Arena will **ONLY** support external engines via UCI
   
   **Rationale:**
   - `TestSuite` (in `src/enginetest/`) already tests built-in engine via direct API
   - Arena's purpose is **version comparison** and **regression testing**
   - Simpler code - single code path, no branching
   - Consistent interface - all engines tested identically
   - Fair comparison - no advantage/disadvantage from test method
   - Dogfooding - tests our UCI implementation as users would
   - Clearer separation: TestSuite = internal API, Arena = external UCI
   
   **Impact on Configuration:**
   - `enginePath` is **always required** (no more empty = built-in)
   - Current version also specified as external engine
   - Simplifies validation and routing logic
   
   **Example:**
   ```yaml
   testSuites:
     - name: "v1.1_WAC"
       epdPath: "test/testsets/wac.epd"
       enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"  # Current
       timePerMove: 5000
       maxDepth: 30
   
     - name: "v1.0_WAC"
       epdPath: "test/testsets/wac.epd"
       enginePath: "Release/v1.0/FrankyCPP_v1.0.exe"  # Old version
       timePerMove: 5000
       maxDepth: 30
   ```

---

### Implementation Summary

**Total Estimated Effort:** 14-18 hours
- Phase 0 (EPD Parser Extraction): 3-4 hours ⚠️ **CRITICAL - Must go first**
- Phases 1-8 (External Engine Support): 11-14 hours

**Key Success Factors:**
1. Phase 0 (EPD Parser) must be completed and validated first
2. Test thoroughly after each phase
3. Never break existing functionality
4. Follow implementation order strictly

**Next Steps:**
1. Decide on built-in vs external-only approach
2. Review revised implementation phases
3. Begin Phase 0 (EPD Parser extraction)
4. Validate Phase 0 thoroughly before proceeding

---

## ✅ Next Steps

### Status: ✅ APPROVED - Ready to Begin Implementation

1. [x] **Decisions Made** - All technical decisions complete
2. [x] **Built-in vs External** - Decided: External-only for Arena
3. [x] **Review Implementation Phases** - All phases reviewed and approved ✅
4. [x] **Approval to Proceed** - APPROVED - Ready for implementation ✅

### 🚀 Implementation Plan:

**Next Session Actions:**
1. Commit this approved design document to current branch
2. Create new feature branch: `feature/external-engine-testsuite-support`
3. Begin Phase 0 implementation (EPD Parser extraction)

### Implementation Sequence (Approved):

1. **Phase 0: EPD Parser Extraction** (3-4 hours) ⚠️ **START HERE**
   - Extract to `src/common/EPDParser.{h,cpp}`
   - Refactor TestSuite to use shared parser
   - **CRITICAL: Validate all tests still pass**
   - Do not proceed until this is 100% working

2. **Phase 1: Configuration** (30 min - 1 hour)
   - Add required enginePath field (no default)
   - Update YAML parsing
   - Validation: enginePath must exist
   - Simple, low-risk

3. **Phases 2-3: UCIEngine** (5-7 hours)
   - Core UCI communication
   - Search implementation
   - Can test independently

4. **Phase 4: Move Comparison** (1-2 hours)
   - Long algebraic + SAN support
   - Reuse internal code

5. **Phase 5: TestSuiteRunner** (2-3 hours)
   - Implement external-only test execution
   - BM/AM/DM support
   - No mode routing needed - always UCI

6. **Phase 6: Results** (1 hour)
   - Add metadata fields (enginePath, engineVersion)
   - Update JSON output

7. **Phase 7: Testing** (2-3 hours)
   - Integration tests
   - Error handling
   - Validation

8. **Phase 8: Documentation** (1 hour)
   - Update configs
   - Code comments
   - User guide

**Total: 14-18 hours**

---

*Discussion document created: 2026-02-01*  
*Discussion document updated: 2026-02-01 - All decisions complete*  
*Approved: 2026-02-01 - All phases approved*  
*Status: **✅ APPROVED - READY FOR IMPLEMENTATION***  
*Next Action: Commit this document, create feature branch, begin Phase 0 (EPD Parser extraction)*
