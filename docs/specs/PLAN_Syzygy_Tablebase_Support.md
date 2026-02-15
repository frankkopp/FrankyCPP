# Plan: Syzygy Tablebase Support

**Status:** Phase 4 Complete - Search Probing Verified  
**Created:** 2026-02-14  
**Updated:** 2026-02-15 (v1.5 - Phase 4 complete, build and tests pass)  
**Priority:** HIGH (Phase 5 in V1_ENGINE_ENHANCEMENT_PLAN)  
**Target Version:** v1.5  
**Expected Impact:** +35-60 ELO (in tablebase-relevant endgames)

---

## Executive Summary

Implement Syzygy tablebase support for perfect endgame play in positions with 6 or fewer pieces (optionally 7). This provides winning/drawing/losing information and optimal moves in endgames, eliminating errors in positions where perfect play is computable.

### Key Benefits
- **Perfect endgame play** - No evaluation errors in TB positions
- **Immediate wins/draws** - Know exact game outcome before searching
- **Faster search** - Cut off branches with known outcomes
- **Tournament advantage** - Essential feature for competitive engines

### Key Challenges
- **Library integration** - Fathom C library must be wrapped for C++
- **Storage & distribution** - TBs are large (150GB+), need download/management strategy
- **Thread safety** - TB probing must be safe for future multi-threaded search
- **Memory management** - TB caching and memory limits
- **Graceful degradation** - Engine must work without TBs available

---

## Background

### What are Syzygy Tablebases?

Syzygy tablebases are endgame databases created by Ronald de Man that store:
- **WDL (Win/Draw/Loss)** - Game outcome with perfect play
- **DTZ (Distance to Zeroing)** - Moves until capture or pawn push

Key characteristics:
- Compressed format (~150GB for 6-piece, ~17TB for 7-piece)
- Two file types: `.rtbw` (WDL) and `.rtbz` (DTZ)
- Standard in all strong engines (Stockfish, Komodo, Leela, etc.)

### Fathom Library

[Fathom](https://github.com/jdart1/Fathom) is the standard C library for Syzygy probing:
- Clean C API
- Thread-safe (with proper configuration)
- Used by Stockfish, Ethereal, many others
- Actively maintained

---

## Implementation Plan

### Phase 1: Fathom Library Integration (2-3 days)

#### 1.1 Add Fathom to Build System

**Note:** Fathom is NOT available in vcpkg (verified 2026-02-14).

**Approach: CMake FetchContent (Recommended)**

Use CMake's `FetchContent` module to download Fathom automatically during configuration:

```cmake
# cmake/Fathom.cmake

include(FetchContent)

FetchContent_Declare(
  fathom
  GIT_REPOSITORY https://github.com/jdart1/Fathom.git
  GIT_TAG        master  # Or pin to specific commit for reproducibility
)

FetchContent_MakeAvailable(fathom)

# Fathom is a C library without CMake support, so we create a target manually
FetchContent_GetProperties(fathom)
if(NOT fathom_POPULATED)
  FetchContent_Populate(fathom)
endif()

add_library(fathom STATIC
  ${fathom_SOURCE_DIR}/src/tbprobe.c
)

target_include_directories(fathom PUBLIC
  ${fathom_SOURCE_DIR}/src
)

# Fathom configuration - thread safety
target_compile_definitions(fathom PRIVATE
  TB_NO_THREADS=0  # Enable thread safety
)
```

**Benefits of FetchContent:**
- Version control - pin to specific commit/tag for reproducibility
- No source code committed to repo - keeps repo clean and small
- Reproducible builds - everyone gets the same version automatically
- Easy updates - change one line to update Fathom version

**Integration in main CMakeLists.txt:**
```cmake
# In src/CMakeLists.txt or main CMakeLists.txt
include(cmake/Fathom.cmake)

# Link to the tablebase module
target_link_libraries(tablebase PRIVATE fathom)
```

**File structure after FetchContent:**
```
src/
  tablebase/
    Tablebase.h       # C++ wrapper
    Tablebase.cpp
    TablebasePaths.h
    TablebasePaths.cpp
# Fathom source is downloaded to cmake build directory automatically
```

#### 1.2 Create C++ Wrapper Class

```cpp
// src/tablebase/Tablebase.h
#ifndef FRANKYCPP_TABLEBASE_H
#define FRANKYCPP_TABLEBASE_H

#include "types/types.h"
#include "chesscore/Position.h"
#include <string>
#include <optional>

namespace tablebase {

/// Result of a tablebase probe
enum class TBResult : int8_t {
  TB_LOSS        = -2,  // Position is lost
  TB_BLESSED_LOSS = -1, // Cursed win for opponent (50-move rule)
  TB_DRAW        = 0,   // Position is drawn
  TB_CURSED_WIN  = 1,   // Blessed loss for opponent (50-move rule)
  TB_WIN         = 2,   // Position is won
  TB_FAILED      = 3    // Probe failed (position not in TB)
};

/// Contains full tablebase probe result with WDL and optional DTZ/move
struct TBProbeResult {
  TBResult wdl{TBResult::TB_FAILED};
  int dtz{0};                         // Distance to zeroing (0 if unavailable)
  Move bestMove{MOVE_NONE};           // Best move (MOVE_NONE if not probed)
  
  [[nodiscard]] bool success() const { return wdl != TBResult::TB_FAILED; }
  [[nodiscard]] bool isWin() const { return wdl == TBResult::TB_WIN || wdl == TBResult::TB_CURSED_WIN; }
  [[nodiscard]] bool isDraw() const { return wdl == TBResult::TB_DRAW; }
  [[nodiscard]] bool isLoss() const { return wdl == TBResult::TB_LOSS || wdl == TBResult::TB_BLESSED_LOSS; }
};

/// Syzygy tablebase probing interface
class Tablebase {
  bool initialized_{false};
  int maxPieces_{0};          // Maximum pieces available (e.g., 6 or 7)
  std::string tbPath_;
  
public:
  Tablebase() = default;
  ~Tablebase();
  
  // Disallow copies
  Tablebase(const Tablebase&) = delete;
  Tablebase& operator=(const Tablebase&) = delete;
  
  /// Initialize tablebases from path. Thread-safe.
  /// @param path  Semicolon-separated list of directories (Windows: semicolon, Linux: colon)
  /// @return true if at least one tablebase was found
  bool initialize(const std::string& path);
  
  /// Shut down and release resources
  void shutdown();
  
  /// Check if tablebases are available
  [[nodiscard]] bool isAvailable() const { return initialized_ && maxPieces_ > 0; }
  
  /// Get maximum pieces available
  [[nodiscard]] int maxPieces() const { return maxPieces_; }
  
  /// Probe WDL only (faster, suitable for search)
  /// @param pos  Position to probe
  /// @return WDL result or TB_FAILED
  [[nodiscard]] TBResult probeWDL(const Position& pos) const;
  
  /// Probe WDL and DTZ (slower, suitable for root)
  /// @param pos  Position to probe
  /// @return Full probe result including DTZ and best move
  [[nodiscard]] TBProbeResult probeRoot(const Position& pos) const;
  
  /// Check if position is probeable (piece count within limit)
  [[nodiscard]] bool canProbe(const Position& pos) const;
  
  /// Convert TBResult to centipawn value for search
  /// @param result  WDL result
  /// @param ply     Current ply (for mate-distance scoring)
  /// @return Value in centipawns
  [[nodiscard]] static Value tbValueToScore(TBResult result, Depth ply);
  
  /// Get string representation of result
  [[nodiscard]] static std::string resultToString(TBResult result);
};

} // namespace tablebase

#endif // FRANKYCPP_TABLEBASE_H
```

#### 1.3 Implement Position Conversion

Fathom requires position data in a specific format. Create conversion functions:

```cpp
// In Tablebase.cpp
namespace {

// Convert FrankyCPP position to Fathom format
unsigned convertPosition(const Position& pos,
                         unsigned& white, unsigned& black,
                         unsigned& kings, unsigned& queens,
                         unsigned& rooks, unsigned& bishops,
                         unsigned& knights, unsigned& pawns,
                         unsigned& ep, bool& turn, unsigned& rule50) {
  white   = pos.getPieceBb(WHITE, PT_ALL).bb();
  black   = pos.getPieceBb(BLACK, PT_ALL).bb();
  kings   = (pos.getPieceBb(WHITE, KING) | pos.getPieceBb(BLACK, KING)).bb();
  queens  = (pos.getPieceBb(WHITE, QUEEN) | pos.getPieceBb(BLACK, QUEEN)).bb();
  rooks   = (pos.getPieceBb(WHITE, ROOK) | pos.getPieceBb(BLACK, ROOK)).bb();
  bishops = (pos.getPieceBb(WHITE, BISHOP) | pos.getPieceBb(BLACK, BISHOP)).bb();
  knights = (pos.getPieceBb(WHITE, KNIGHT) | pos.getPieceBb(BLACK, KNIGHT)).bb();
  pawns   = (pos.getPieceBb(WHITE, PAWN) | pos.getPieceBb(BLACK, PAWN)).bb();
  
  ep = pos.getEnPassantSquare() != SQ_NONE ? pos.getEnPassantSquare() : 0;
  turn = pos.getNextPlayer() == WHITE;
  rule50 = pos.getHalfMoveClock();
  
  return pos.getPieceCount();  // Total piece count
}

// Convert Fathom move to FrankyCPP Move
Move convertMove(unsigned fathomMove, const Position& pos) {
  if (fathomMove == TB_RESULT_FAILED) return MOVE_NONE;
  
  const Square from = Square(TB_GET_FROM(fathomMove));
  const Square to   = Square(TB_GET_TO(fathomMove));
  const unsigned promo = TB_GET_PROMOTES(fathomMove);
  
  // Handle promotions
  if (promo != TB_PROMOTES_NONE) {
    PieceType pt = KNIGHT;  // Default
    switch (promo) {
      case TB_PROMOTES_QUEEN:  pt = QUEEN; break;
      case TB_PROMOTES_ROOK:   pt = ROOK; break;
      case TB_PROMOTES_BISHOP: pt = BISHOP; break;
      case TB_PROMOTES_KNIGHT: pt = KNIGHT; break;
    }
    return Move::promotion(from, to, pt, 0);
  }
  
  // Handle en passant
  if (pos.getPiece(from).type() == PAWN && to == pos.getEnPassantSquare()) {
    return Move::enPassant(from, to, 0);
  }
  
  // Handle castling
  if (pos.getPiece(from).type() == KING && std::abs(from - to) == 2) {
    return Move::castling(from, to, 0);
  }
  
  // Normal move
  return Move::normal(from, to, 0);
}

} // anonymous namespace
```

---

### Phase 2: Storage & Download Management (2-3 days)

**Rationale:** Tablebases must be available before any probing code can be tested. This phase ensures developers and CI have access to TB files.

#### 2.1 Storage Strategy

**Do NOT bundle TBs with the build** - they're too large (150GB+ for 6-piece).

**Path Resolution Order (highest to lowest priority):**
1. UCI `SyzygyPath` option
2. YAML config `TB_PATH`
3. Environment variable `SYZYGY_PATH`
4. Platform defaults:
   - Windows: `%LOCALAPPDATA%\FrankyCPP\syzygy\`
   - Linux: `~/.local/share/frankycpp/syzygy/`

```cpp
// src/tablebase/TablebasePaths.h
namespace tablebase {

/// Returns the first valid TB path found, or empty string
std::string findTablebasePath();

/// Get platform-specific default TB directory
std::string getDefaultTablebasePath();

/// Check if a path contains valid TB files
bool validateTablebasePath(const std::string& path);

} // namespace tablebase
```

#### 2.2 Tablebase Tiers

| Tier            | Pieces    | Size    | Use Case                |
|-----------------|-----------|---------|-------------------------|
| **Minimal**     | 3-4       | ~7 MB   | Unit tests, CI          |
| **Development** | 3-4-5     | ~1 GB   | Full dev testing        |
| **Standard**    | 3-4-5-6   | ~150 GB | Production, tournaments |
| **Complete**    | 3-4-5-6-7 | ~17 TB  | Maximum strength        |

#### 2.3 CLI Download Feature

Add `--syzygy` command group:

```powershell
# Check status of local tablebases
FrankyCPP --syzygy status

# Download specific piece counts to default location
FrankyCPP --syzygy download --pieces 3-4-5

# Download to custom path
FrankyCPP --syzygy download --pieces 3-4-5-6 --path D:\Chess\Syzygy

# Verify integrity of downloaded files
FrankyCPP --syzygy verify

# Show download progress
FrankyCPP --syzygy download --pieces 6 --verbose
```

**Implementation:**

```cpp
// src/tablebase/TablebaseDownloader.h
namespace tablebase {

struct DownloadConfig {
  std::vector<int> pieceCounts;  // e.g., {3, 4, 5}
  std::string targetPath;
  bool verifyChecksums{true};
  bool verbose{false};
};

struct DownloadProgress {
  std::string currentFile;
  size_t bytesDownloaded{0};
  size_t totalBytes{0};
  int filesCompleted{0};
  int totalFiles{0};
};

using ProgressCallback = std::function<void(const DownloadProgress&)>;

class TablebaseDownloader {
public:
  /// Download tablebases for specified piece counts
  /// @return true if all downloads succeeded
  bool download(const DownloadConfig& config, ProgressCallback progress = nullptr);
  
  /// Verify integrity of existing TB files
  bool verify(const std::string& path, int maxPieces = 6);
  
  /// Get list of files needed for given piece counts
  std::vector<std::string> getRequiredFiles(const std::vector<int>& pieceCounts);
  
  /// Get total download size in bytes
  size_t estimateDownloadSize(const std::vector<int>& pieceCounts);
  
private:
  static constexpr const char* PRIMARY_MIRROR = "http://tablebase.sesse.net/syzygy/";
  static constexpr const char* BACKUP_MIRROR = "https://tablebase.lichess.ovh/tables/standard/";
};

} // namespace tablebase
```

#### 2.4 Download Sources

| Source  | URL                                              | Notes                            |
|---------|--------------------------------------------------|----------------------------------|
| Primary | `http://tablebase.sesse.net/syzygy/`             | Ronald de Man's server           |
| Lichess | `https://tablebase.lichess.ovh/tables/standard/` | Fast CDN                         |
| Torrent | Various                                          | Best for 6-7 piece bulk download |

#### 2.5 Development & CI Setup ✅

**For Development Team:** (Not implemented - user-specific configuration)
```yaml
# Option 1: Shared network location
SYZYGY_PATH: "\\\\server\chess\syzygy\3-4-5-6"

# Option 2: Local with auto-download on first run
TB_AUTO_DOWNLOAD: true
TB_AUTO_DOWNLOAD_PIECES: "3-4-5"
```

**For CI/GitHub Actions:** ✅ **Implemented in `.github/workflows/ci-build.yml`**
- Added `actions/cache@v4` for Syzygy 3-4 piece tablebases (key: `syzygy-3-4-v1`)
- Downloads WDL files from Lichess CDN when cache miss
- Sets `SYZYGY_PATH` environment variable for test runs
- Implemented for both Windows and Linux builds

```yaml
# .github/workflows/ci-build.yml (actual implementation)
- name: Cache Syzygy 3-4 piece tablebases
  uses: actions/cache@v4
  with:
    path: ./test_syzygy
    key: syzygy-3-4-v1

- name: Download test tablebases
  if: steps.cache-syzygy-*.outputs.cache-hit != 'true'
  run: # Downloads 3-4 piece WDL files from Lichess CDN
    
- name: Run tests
  env:
    SYZYGY_PATH: ${{ github.workspace }}/test_syzygy
  run: ctest --output-on-failure
```

#### 2.6 Graceful Skip in Tests ✅

**Implemented in `test/tablebase/TablebaseTest.cpp`:**
- `TablebaseIntegrationTest` class uses `skipIfNoTablebases()` helper
- Uses `GTEST_SKIP()` with helpful message when TBs unavailable
- Path resolution supports both `SYZYGY_PATH` (standard) and `TB_PATH` (legacy)

```cpp
// test/tablebase/TablebaseTest.cpp (actual implementation)
void skipIfNoTablebases() const {
  if (!tb.isAvailable()) {
    GTEST_SKIP() << "Syzygy tablebases not available. "
                 << "Set SYZYGY_PATH environment variable or configure TB_PATH in search.yaml";
  }
}
```

---

### Phase 3: Root Tablebase Probing (2-3 days) ✅ COMPLETE

Probe tablebases at the root position before starting search.

**Implementation Status (2026-02-15):**
- ✅ Added `syzygy_tb` member to `Search` class
- ✅ Added `initTablebase()` method - initializes TB from `TB_PATH` config
- ✅ Added `probeTablebaseAtRoot()` method - probes TB and returns result
- ✅ Integrated root probe in `iterativeDeepening()` before search loop
- ✅ Added `tbHit` field to `SearchResult`
- ✅ Added `tbRootHits` statistic to `SearchStats`
- ✅ UCI info string output on TB hit
- ✅ Added `TB_ROOT_IMMEDIATE` config option (default: **false**)
  - `true`: Return TB move immediately without searching
  - `false`: Filter root moves, search for PV, guarantee TB-optimal play
- ✅ Added `tbRootMove`, `tbRootValue`, `tbRootWdl`, `tbRootDtz` members
- ✅ **DTZ-based scoring** via `tbResultToScore(wdl, dtz)`:
  - Shorter wins (smaller DTZ) score higher
  - Score range: ~8800 (DTZ=200) to ~8999 (DTZ=1) for wins
- ✅ **Root move filtering** via `filterRootMovesByTB()`:
  - Winning position: Keep only moves where opponent is losing
  - Drawing position: Keep only moves where opponent is not winning
  - Losing position: Keep all moves (best effort)
  - Probes WDL for each child position to verify
- ✅ **Smart move selection** after search:
  - Default: Use TB move (DTZ-optimal, avoids 50-move rule issues)
  - Override: If search found a **proven mate** at least as short as TB's DTZ
  - Proven mates always preferred (uses `<=` comparison with DTZ)
  - Ensures mate scores (e.g., 9999 for mate-in-1) are preserved

#### ⚠️ PERFORMANCE IMPLEMENTATION GUIDELINES

While root probing itself is **NOT hot code** (called once per search), the patterns established here will be reused in **Phase 4 (search probing) which IS hot code**. Every nanosecond counts in chess engine hot paths. Implement with extreme care:

| Concern                 | Guideline                                                                                                                |
|-------------------------|--------------------------------------------------------------------------------------------------------------------------|
| **Redundant checks**    | Avoid double `canProbe()` calls - if caller verified, trust it. Consider `_unchecked` internal variants.                 |
| **Position conversion** | `convertPositionToFathom()` costs ~50-70 cycles. Acceptable, but add no unnecessary overhead.                            |
| **EP legality check**   | Current check is correct (verifies pawn existence only, NOT king-in-check). Fathom handles full legality internally.     |
| **Move conversion**     | `convertFathomMove()` is needed ONLY for root probing (best move). Search probing returns WDL only - no move conversion. |
| **Profiling**           | Profile before/after integration. Measure with 3-4-5 piece TBs under realistic search conditions.                        |

#### 3.1 Integration Points

**In `Search::iterativeDeepening()` before search loop:**

```cpp
SearchResult Search::iterativeDeepening(Position& p) {
  // ... existing setup ...
  
  // Probe tablebase at root
  if (SearchConfig.USE_TABLEBASES && tb->canProbe(p)) {
    const auto tbResult = tb->probeRoot(p);
    if (tbResult.success() && tbResult.bestMove != MOVE_NONE) {
      // Found TB move - can return immediately or use for scoring
      LOG_INFO << "TB hit at root: " << tablebase::Tablebase::resultToString(tbResult.wdl)
               << " DTZ=" << tbResult.dtz
               << " move=" << tbResult.bestMove.str();
      
      if (SearchConfig.TB_ROOT_IMMEDIATE) {
        // Return TB move immediately (fastest, but no PV)
        return SearchResult{tbResult.bestMove, 
                           tablebase::Tablebase::tbValueToScore(tbResult.wdl, 0),
                           0};  // depth 0 = TB
      } else {
        // Use TB for move scoring in root search
        tbRootMove = tbResult.bestMove;
        tbRootScore = tablebase::Tablebase::tbValueToScore(tbResult.wdl, 0);
      }
    }
  }
  
  // ... continue with normal search ...
}
```

#### 3.2 Root Move Scoring with TB

When TB is available but we want a PV, score root moves using TB:

```cpp
Value Search::rootSearch(Position& p, Depth depth, Value alpha, Value beta) {
  // ... existing root search code ...
  
  for (const auto& move : rootMoves) {
    // Check if this is the TB best move
    if (SearchConfig.USE_TABLEBASES && move == tbRootMove) {
      // Give significant bonus to TB move
      // This ensures it gets searched first and selected
    }
    
    // ... rest of root search ...
  }
}
```

---

### Phase 4: Search Tablebase Probing (1 week) 🔄 IMPLEMENTED

Probe tablebases during search to cut off branches with known outcomes.

**Status:** Implemented (2026-02-15) - awaiting build and test verification

#### 4.0 Key Differences from Phase 3 (Root Probing)

| Aspect          | Phase 3 (Root)                   | Phase 4 (Search)                  |
|-----------------|----------------------------------|-----------------------------------|
| **Frequency**   | Once per search                  | Thousands/millions per search     |
| **Performance** | Not critical                     | **HOT CODE** - every cycle counts |
| **Probing**     | `probeRoot()` (WDL + DTZ + move) | `probeWDL()` only (faster)        |
| **Threading**   | Single-threaded                  | Must remain thread-safe           |
| **Use case**    | Best move selection              | Branch cutoffs                    |

#### 4.1 When to Probe

**Key principle:** Only probe when it's likely to help and overhead is acceptable.

**Fast rejection order (cheapest checks first):**
1. `depth >= TB_PROBE_DEPTH` - skip shallow nodes
2. `pieceCount <= TB_PROBE_LIMIT` - fast popcount check
3. `castlingRights == NO_CASTLING` - fast bitwise check
4. Call `probeWDL()` only after all checks pass

```cpp
// In Search::search() early, after legal move check
if (depth >= SearchConfig.TB_PROBE_DEPTH && canProbeTablebaseInSearch(pos)) {
  const TBResult wdl = syzygy_tb->probeWDL(pos);
  
  if (wdl != TBResult::Failed) {
    statistics.tbSearchHits++;
    
    // Get TB score (with 50-move rule adjustment if needed)
    const Value tbScore = getTBScoreForSearch(wdl, pos.getHalfMoveClock(), ply);
    
    // Use as bound for alpha-beta
    if (wdl == TBResult::Win || wdl == TBResult::CursedWin) {
      // Position is winning - use as lower bound
      if (tbScore >= beta) {
        statistics.tbCutoffs++;
        return tbScore;  // Fail high
      }
      alpha = std::max(alpha, tbScore);
    } 
    else if (wdl == TBResult::Loss || wdl == TBResult::BlessedLoss) {
      // Position is losing - use as upper bound
      if (tbScore <= alpha) {
        statistics.tbCutoffs++;
        return tbScore;  // Fail low
      }
      beta = std::min(beta, tbScore);
    } 
    else {
      // Draw - exact value
      return tbScore;
    }
  } else {
    statistics.tbSearchMisses++;
  }
}
```

#### 4.2 Configuration Options

```yaml
# config/search.yaml - Phase 4 additions
TB_PROBE_DEPTH: 1       # Minimum remaining depth to probe (0 = always, higher = less probing)
TB_PROBE_LIMIT: 6       # Max pieces for search probing (may be < root limit for speed)
TB_RULE50_THRESHOLD: 80 # HalfMoveClock threshold for DTZ check (>=100 disables DTZ checks)
```

**TB_RULE50_THRESHOLD behavior:**
- `< 100`: When `halfMoveClock >= threshold`, probe DTZ to check if win/loss is achievable
- `>= 100`: Effectively disables DTZ checks (since halfMoveClock maxes at 100)
- **Default: 80** - balances accuracy vs performance (only ~20% of positions need DTZ check)

#### 4.3 50-Move Rule Handling (Optimized)

The 50-move rule complicates TB scoring. If `halfMoveClock + DTZ > 100`, a theoretical win/loss becomes a draw.

**Two-tier approach for performance:**

```cpp
Value getTBScoreForSearch(TBResult wdl, int halfMoveClock, Depth ply) {
  // Fast path: far from 50-move limit, no DTZ needed
  if (halfMoveClock < SearchConfig.TB_RULE50_THRESHOLD) {
    return tbValueToScore(wdl, ply);  // Use simple WDL scoring
  }
  
  // Slow path: near 50-move limit, need DTZ for accuracy
  // Only triggered when halfMoveClock >= TB_RULE50_THRESHOLD (default 80)
  if (wdl == TBResult::Win || wdl == TBResult::Loss) {
    // Option A: Probe DTZ separately (if Fathom exposes it)
    // Option B: Be conservative - treat uncertain wins/losses as draws
    // Option C: Call probeRoot() for DTZ (slower but accurate)
    
    // Conservative approach (recommended for search):
    // If we can't verify the win is achievable, treat as draw
    // This is safe - we won't miss wins, just won't cut as aggressively
    if (halfMoveClock >= 90) {
      // Very close to 50-move, be extra conservative
      return VALUE_DRAW;
    }
    // Between threshold and 90: use WDL but with reduced confidence
    return tbValueToScore(wdl, ply);
  }
  
  // CursedWin/BlessedLoss/Draw: return as-is
  return tbValueToScore(wdl, ply);
}
```

**Why this works:**
- Most positions have low halfMoveClock (resets on capture/pawn move)
- Only ~1-5% of search positions will hit the slow path
- Conservative scoring is safe: we never claim a win we can't achieve
- `TB_RULE50_THRESHOLD >= 100` completely disables this check for maximum speed

#### 4.4 Statistics Additions

Add to `SearchStats`:

```cpp
// SearchStats.h additions
uint64_t tbSearchHits   = 0;  // Successful WDL probes during search
uint64_t tbSearchMisses = 0;  // Positions where probe returned Failed
uint64_t tbCutoffs      = 0;  // Branches cut off due to TB result
```

#### 4.5 Helper Function

```cpp
// In Search class (private)
[[nodiscard]] bool canProbeTablebaseInSearch(const Position& pos) const {
  // Fast rejection checks in order of cost
  if (!syzygy_tb || !syzygy_tb->isAvailable()) return false;
  if (pos.getOccupiedBb().popcount() > SearchConfig.TB_PROBE_LIMIT) return false;
  if (pos.getCastlingRights() != NO_CASTLING) return false;
  return true;
}
```

#### 4.6 Integration Point

Primary insertion point in `Search::search()`:

```cpp
Value Search::search(Position& p, Depth depth, Value alpha, Value beta, ...) {
  // ... node count, stop check, draw detection ...
  
  // Tablebase probing in search (early cutoff)
  if (depth >= SearchConfig.TB_PROBE_DEPTH && canProbeTablebaseInSearch(p)) {
    // ... probing code from 4.1 ...
  }
  
  // ... TT probe, null move, etc. ...
}
```

**Placement rationale:** After basic checks but before expensive operations (TT probe, move generation). TB cutoff is often conclusive and avoids wasted work.

#### 4.7 TT Integration

TB results should be stored in TT for efficiency:

```cpp
// After successful TB probe with cutoff
if (tbScore >= beta) {
  // Store as lower bound in TT
  tt->store(pos.getZobristKey(), depth, tbScore, TT_LOWER_BOUND, MOVE_NONE);
  return tbScore;
}
```

This prevents re-probing the same position if TT entry survives.

#### 4.8 Testing Plan

1. **Unit tests:**
   - Verify `canProbeTablebaseInSearch()` rejection logic
   - Verify `getTBScoreForSearch()` with various halfMoveClock values
   - Verify TB_RULE50_THRESHOLD >= 100 disables DTZ checks

2. **Integration tests:**
   - Verify cutoffs occur correctly in known TB positions
   - Verify search doesn't break when TB unavailable

3. **Benchmark tests:**
   - Measure NPS impact in non-TB positions (target: <5% regression)
   - Measure NPS improvement in TB positions

4. **Strength tests:**
   - Run endgame suite (e.g., WAC endgames, Nunn positions)
   - Verify ELO improvement in TB-relevant positions

#### 4.9 Performance Considerations

| Concern               | Guideline                                    |
|-----------------------|----------------------------------------------|
| **Probe frequency**   | Use `TB_PROBE_DEPTH >= 1` to skip leaf nodes |
| **Piece count check** | Single popcount (~1 cycle) - very fast       |
| **Castling check**    | Bitwise compare (~1 cycle) - very fast       |
| **probeWDL() cost**   | ~10-50μs typical, acceptable at depth >= 1   |
| **TT caching**        | Store TB results to avoid re-probing         |
| **50-move check**     | Only when halfMoveClock >= threshold         |

#### 4.10 Estimated Effort

- Implementation: 2-3 days
- Testing & validation: 2 days
- Performance tuning: 1-2 days
- **Total: ~1 week**

#### 4.11 Success Criteria

- [ ] TB probing integrated in `Search::search()`
- [ ] `TB_PROBE_DEPTH`, `TB_PROBE_LIMIT`, `TB_RULE50_THRESHOLD` configs working
- [ ] Statistics tracking (tbSearchHits, tbSearchMisses, tbCutoffs)
- [ ] All existing tests pass
- [ ] NPS regression < 5% in non-TB positions
- [ ] Measurable cutoffs in TB endgame positions

---

### Phase 5: Configuration & UCI (2-3 days)

#### 5.1 SearchConfigData Additions

```cpp
// src/config/SearchConfigData.h

// Tablebase settings
bool USE_TABLEBASES      = false;           // Master switch
std::string TB_PATH      = "";              // Path to TB files (empty = disabled)
int TB_PROBE_DEPTH       = 1;               // Min depth to probe in search
int TB_PROBE_LIMIT       = 6;               // Max pieces (6 or 7)
bool TB_ROOT_IMMEDIATE   = false;           // Return TB move immediately at root
bool TB_ROOT_IN_SEARCH   = true;            // Use TB for root move scoring
int TB_CACHE_MB          = 32;              // Internal cache size (Fathom)
```

#### 5.2 ConfigRegistry Additions

```cpp
// src/config/ConfigRegistry.cpp - in initializeSearchDefinitions()

{
  .name = "USE_TABLEBASES",
  .uciName = "SyzygyEnabled",
  .description = "Enable Syzygy tablebase probing",
  .valueType = ConfigValueType::Bool,
  .domain = ConfigDomain::Search,
  .defaultValue = "false",
  .exposure = {.uci = true, .yaml = true, .display = true},
  .getter = [](const auto& s, const auto&) { return configToString(s.USE_TABLEBASES); },
  .setter = [](auto& s, auto&, const std::string& v) { s.USE_TABLEBASES = parseBool(v); }
},
{
  .name = "TB_PATH",
  .uciName = "SyzygyPath",
  .description = "Path to Syzygy tablebase files (semicolon-separated on Windows)",
  .valueType = ConfigValueType::String,
  .domain = ConfigDomain::Search,
  .defaultValue = "",
  .exposure = {.uci = true, .yaml = true, .display = true},
  .getter = [](const auto& s, const auto&) { return s.TB_PATH; },
  .setter = [](auto& s, auto&, const std::string& v) { s.TB_PATH = v; }
},
{
  .name = "TB_PROBE_DEPTH",
  .uciName = "SyzygyProbeDepth",
  .description = "Minimum remaining depth to probe TB in search",
  .valueType = ConfigValueType::Int,
  .domain = ConfigDomain::Search,
  .defaultValue = "1",
  .minValue = 0,
  .maxValue = 10,
  .exposure = {.uci = true, .yaml = true, .display = true},
  .getter = [](const auto& s, const auto&) { return configToString(s.TB_PROBE_DEPTH); },
  .setter = [](auto& s, auto&, const std::string& v) { s.TB_PROBE_DEPTH = parseInt(v); }
},
{
  .name = "TB_PROBE_LIMIT",
  .uciName = "SyzygyProbeLimit",
  .description = "Maximum pieces for tablebase probing (6 or 7)",
  .valueType = ConfigValueType::Int,
  .domain = ConfigDomain::Search,
  .defaultValue = "6",
  .minValue = 3,
  .maxValue = 7,
  .exposure = {.uci = true, .yaml = true, .display = true},
  .getter = [](const auto& s, const auto&) { return configToString(s.TB_PROBE_LIMIT); },
  .setter = [](auto& s, auto&, const std::string& v) { s.TB_PROBE_LIMIT = parseInt(v); }
},
```

#### 5.3 UCI Option Names

Follow Stockfish conventions for compatibility with GUIs:
- `SyzygyPath` - Path to TB files
- `SyzygyProbeDepth` - Min depth to probe
- `SyzygyProbeLimit` - Max pieces
- `Syzygy50MoveRule` - Respect 50-move rule (optional)
- `SyzygyEnabled` - Master switch (FrankyCPP-specific, as Stockfish enables via path)

---

### Phase 6: Testing (2-3 days)

#### 6.1 Unit Tests

```cpp
// test/tablebase/TablebaseTest.cpp

class TablebaseTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }
  
protected:
  tablebase::Tablebase tb;
};

// Test initialization
TEST_F(TablebaseTest, initializeWithValidPath) {
  // Skip if TB not available in test environment
  if (!std::filesystem::exists("./syzygy/")) {
    GTEST_SKIP() << "Syzygy tablebases not available";
  }
  
  EXPECT_TRUE(tb.initialize("./syzygy/"));
  EXPECT_TRUE(tb.isAvailable());
  EXPECT_GE(tb.maxPieces(), 3);
}

TEST_F(TablebaseTest, initializeWithInvalidPath) {
  EXPECT_FALSE(tb.initialize("/nonexistent/path/"));
  EXPECT_FALSE(tb.isAvailable());
}

// Test WDL probing
TEST_F(TablebaseTest, probeKRvK_win) {
  // White: Ke1, Ra1, Black: Ke8 - White wins
  Position pos("8/8/8/8/8/8/8/R3K2k w Q - 0 1");
  
  if (!tb.isAvailable() || !tb.canProbe(pos)) {
    GTEST_SKIP() << "TB not available for this position";
  }
  
  auto result = tb.probeWDL(pos);
  EXPECT_EQ(result, TBResult::TB_WIN);
}

TEST_F(TablebaseTest, probeKvK_draw) {
  // Bare kings - draw
  Position pos("8/8/8/4k3/8/8/8/4K3 w - - 0 1");
  
  if (!tb.isAvailable() || !tb.canProbe(pos)) {
    GTEST_SKIP() << "TB not available for this position";
  }
  
  auto result = tb.probeWDL(pos);
  EXPECT_EQ(result, TBResult::TB_DRAW);
}

// Test DTZ probing
TEST_F(TablebaseTest, probeRootKQvK) {
  // KQ vs K - should find shortest win
  Position pos("8/8/8/8/8/8/8/KQ5k w - - 0 1");
  
  if (!tb.isAvailable() || !tb.canProbe(pos)) {
    GTEST_SKIP() << "TB not available for this position";
  }
  
  auto result = tb.probeRoot(pos);
  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.isWin());
  EXPECT_NE(result.bestMove, MOVE_NONE);
  EXPECT_GT(result.dtz, 0);
}

// Test piece count limit
TEST_F(TablebaseTest, canProbe_respectsLimit) {
  // Position with 7 pieces when limit is 6
  Position pos("8/8/8/8/8/8/PP6/K5Rk w - - 0 1");  // 5 pieces
  
  EXPECT_EQ(pos.getPieceCount(), 5);
  EXPECT_TRUE(tb.canProbe(pos));  // Under limit
}
```

#### 6.2 Integration Tests

```cpp
// Test TB integration with search
TEST_F(SearchTest, tbRootProbe_returnsCorrectMove) {
  // KR vs K position
  Position pos("8/8/8/8/8/8/8/R3K2k w Q - 0 1");
  
  Search search;
  SearchLimits limits;
  limits.depth = 1;  // Minimal depth, TB should take over
  
  search.startSearch(pos, limits);
  search.waitWhileSearching();
  
  const auto& result = search.getLastSearchResult();
  // Should find winning continuation
  EXPECT_TRUE(result.bestMove != MOVE_NONE);
  // Score should indicate winning
  EXPECT_GT(result.score, VALUE_KNOWN_WIN - 100);
}
```

#### 6.3 Benchmark Tests

```cpp
// Test TB probing performance
TEST_F(TablebaseBenchmark, probeWDL_latency) {
  // Measure average probe time
  std::vector<Position> positions = loadTBPositions();
  
  auto start = std::chrono::high_resolution_clock::now();
  for (const auto& pos : positions) {
    tb.probeWDL(pos);
  }
  auto end = std::chrono::high_resolution_clock::now();
  
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  double avgMicros = static_cast<double>(duration.count()) / positions.size();
  
  // Should be < 100μs average
  EXPECT_LT(avgMicros, 100.0);
  
  std::cout << "Average TB probe time: " << avgMicros << " μs" << std::endl;
}
```

---

### Phase 7: Cache Optimization (Optional, 2-3 days)

#### 7.1 Fathom Internal Cache

Fathom has its own caching. Configure appropriately:

```cpp
bool Tablebase::initialize(const std::string& path) {
  // Initialize with cache size
  const bool success = tb_init(path.c_str());
  
  // Note: Fathom doesn't expose cache size configuration
  // Cache is managed internally
  
  return success;
}
```

#### 7.2 Position-Based Caching - Not Recommended

**Analysis:** A separate TB result cache is **not needed** because the standard Transposition Table (TT) already handles this:

- TB probe returns a score → stored in TT as exact bound
- Same position revisited → TT hit, no re-probe needed
- Zobrist key matching already implemented

A dedicated cache would only help if:
1. TT replacement pressure evicts TB entries (rare - TB positions have few pieces, limited branching)
2. Cross-search persistence needed (TT can be configured for this)
3. Very frequent re-probes of identical positions (TB probes are fast: ~10-50μs)

**Conclusion:** The complexity of a separate cache is not justified. Fathom's internal file I/O cache (Phase 7.1) combined with the existing TT provides sufficient caching.

---

## File Structure

```
src/
  tablebase/
    Tablebase.h           # C++ wrapper interface           ✅ Implemented
    Tablebase.cpp         # Implementation                  ✅ Implemented
    TablebasePaths.h      # Path resolution utilities       ✅ Implemented
    TablebasePaths.cpp                                      ✅ Implemented
    TablebaseDownloader.h # Download management             ✅ Implemented
    TablebaseDownloader.cpp                                 ✅ Implemented

cmake/
  Fathom.cmake            # FetchContent configuration      ✅ Implemented

test/
  tablebase/
    TablebaseTest.cpp     # Tablebase unit tests            ✅ Implemented
    TablebasePathsTest.cpp # Path utilities tests           ✅ Implemented
    TablebaseDownloaderTest.cpp # Downloader tests          ✅ Implemented
    
config/
  search.yaml             # TB configuration defaults       ✅ Updated

# Note: Fathom source is automatically downloaded to build directory
# via CMake FetchContent - no vendored source files in repo
```

---

## Configuration Reference

### YAML Configuration

```yaml
# config/search.yaml

# Tablebase Settings
TB_PATH: "D:/Chess/Syzygy/3-4-5-6"  # Windows path (semicolon-separated for multiple)

# Root probing (once per search, for best move selection)
USE_TB_PROBE_ROOT: true              # Enable root probing
TB_ROOT_IMMEDIATE: false             # false = search for PV, true = return TB move immediately

# Search probing (during tree search, for cutoffs)
USE_TB_PROBE_SEARCH: true            # Enable search probing for cutoffs
TB_PROBE_DEPTH: 1                    # Min remaining depth to probe in search (0 = always)
TB_PROBE_LIMIT: 6                    # Max pieces for search probing (3-7)
TB_RULE50_THRESHOLD: 80              # HalfMoveClock threshold for DTZ check (>=100 disables)
```

**TB_RULE50_THRESHOLD explained:**
- When `halfMoveClock >= threshold`, the engine checks DTZ to verify win/loss is achievable
- Values 0-99: Enable 50-move rule accuracy checks when halfMoveClock reaches threshold
- Value >= 100: Disables DTZ checks entirely (maximum speed, may claim unreachable wins)
- **Default: 80** - good balance of accuracy vs performance

### UCI Options

| Option              | Type   | Default | Description                                      |
|---------------------|--------|---------|--------------------------------------------------|
| `SyzygyPath`        | string | ""      | Path to tablebase files                          |
| `SyzygyProbeRoot`   | check  | true    | Enable root probing                              |
| `SyzygyProbeSearch` | check  | true    | Enable search probing for cutoffs                |
| `SyzygyProbeDepth`  | spin   | 1       | Min depth to probe in search (0-10)              |
| `SyzygyProbeLimit`  | spin   | 6       | Max pieces for probing (3-7)                     |
| `Syzygy50MoveRule`  | spin   | 80      | HalfMoveClock threshold for DTZ (0-100, 100=off) |

---

## Dependencies

### Fathom Library

- **Repository:** https://github.com/jdart1/Fathom
- **License:** MIT
- **Version:** Latest stable (pin to specific commit recommended)
- **Integration:** CMake FetchContent (vcpkg not available, verified 2026-02-14)

### Syzygy Tablebases

- **3-4-5 man:** ~1 GB (essential)
- **6 man:** ~150 GB (recommended)
- **7 man:** ~17 TB (optional)

Download from: http://tablebase.sesse.net/ or torrent

---

## Timeline

| Phase | Task                          | Effort   | Status         |
|-------|-------------------------------|----------|----------------|
| 1     | Fathom Library Integration    | 2-3 days | ✅ Complete     |
| 2     | Storage & Download Management | 2-3 days | ✅ Complete     |
| 3     | Root Tablebase Probing        | 2-3 days | ✅ Complete     |
| 4     | Search Tablebase Probing      | 1 week   | ✅ Complete     |
| 5     | Configuration & UCI           | 2-3 days | ✅ Complete     |
| 6     | Testing                       | 2-3 days | ✅ Complete     |
| 7     | Cache Optimization (Optional) | 2-3 days | 📋 Optional    |

**Current State (2026-02-15):**
- Phases 1-6 complete
- **Phase 4 verified** - build passes, TB unit tests pass
- Feature gate pattern: inline conditions at call site (consistent with other features)
- New config options: `TB_PROBE_LIMIT`, `TB_RULE50_THRESHOLD`
- New statistics: `tbSearchHits`, `tbSearchMisses`, `tbSearchCutoffs`
- Search probing integrated in `Search::search()` after TT lookup
- One Franky test updated (FRANKY-1 #6: both Nxf3 moves are TB-optimal wins)

### Phase 1 Completion Notes (2026-02-14)

**Implemented:**
- `cmake/Fathom.cmake` - FetchContent integration (downloads Fathom automatically)
- `src/tablebase/Tablebase.h/.cpp` - Full C++ wrapper with:
  - `TBResult` enum (Win, CursedWin, Draw, BlessedLoss, Loss, Failed)
  - `TBProbeResult` struct with WDL, DTZ, and best move
  - `Tablebase` class with initialize/shutdown, probeWDL, probeRoot, canProbe
  - Position-to-Fathom conversion and Fathom-move-to-Move conversion
- `src/CMakeLists.txt` - Links to fathom library
- `test/tablebase/TablebaseTest.cpp` - Comprehensive unit tests (skips if TBs unavailable)

### Phase 2 Completion Notes (2026-02-14)

**Implemented:**
- `src/tablebase/TablebasePaths.h/.cpp` - Path resolution utilities:
  - `findTablebasePath()` - Priority-based path resolution (env > config > defaults)
  - `getDefaultTablebasePath()` - Platform-specific defaults
  - `validateTablebasePath()` - Checks for .rtbw/.rtbz files
  - `countTablebaseFiles()` - Counts WDL and DTZ files
  - `getTablebaseStatus()` - Human-readable status string
- `src/tablebase/TablebaseDownloader.h/.cpp` - Download management:
  - `download()` - Downloads TB files from mirrors with progress callback
  - `getRequiredFiles()` - Lists files needed for piece counts
  - `estimateDownloadSize()` - Estimates total download size
  - `formatSize()` - Human-readable size formatting
  - `checkStatus()` - Reports status of existing TBs
- CLI commands in `main.cpp`:
  - `--syzygy status` - Show local tablebase status
  - `--syzygy download --pieces 3-4-5 --path D:/SYZYGY` - Download tablebases
- `test/tablebase/TablebasePathsTest.cpp` - Unit tests for path utilities
- `test/tablebase/TablebaseDownloaderTest.cpp` - Unit tests for downloader
- Configuration added to `SearchConfigData`:
  - `TB_PATH` - Path to tablebase files (UCI: `SyzygyPath`)
  - `USE_TB_PROBE_ROOT` - Enable root probing (UCI: `SyzygyProbeRoot`)
  - `USE_TB_PROBE_SEARCH` - Enable search probing (UCI: `SyzygyProbeSearch`)
  - `TB_PROBE_DEPTH` - Minimum depth for search probing (UCI: `SyzygyProbeDepth`)
  - `TB_PROBE_LIMIT` - Maximum pieces for search probing (UCI: `SyzygyProbeLimit`)
  - `TB_RULE50_THRESHOLD` - 50-move rule handling threshold (UCI: `Syzygy50MoveRule`)
- `config/search.yaml` - Default configuration with `TB_PATH: "D:/SYZYGY"`
- Updated `TablebaseTest.cpp` to use `findTablebasePath()` for auto-detection

### External Code Review (2026-02-15)

**Review Process:** Three independent AI engines reviewed the implementation. Key findings addressed:

**Fix 1: En Passant Legality Check (HIGH)**
- **Issue:** EP square was passed to Fathom even when no legal EP capture existed
- **Fix:** Added check using `Bitboards::pawnAttacks[~stm][epSq]` to verify an actual capturing pawn exists
- **File:** `src/tablebase/Tablebase.cpp` - `convertPositionToFathom()`

**Fix 2: Static Asserts for Fathom Compatibility**
- Added compile-time verification that square encoding matches Fathom's expectations
- Verifies: A1=0, H1=7, A8=56, H8=63, SQ_NONE=64
- Verifies EP squares: A3=16, H3=23, A6=40, H6=47
- **File:** `src/tablebase/Tablebase.cpp`

**Fix 3: Improved Documentation**
- Clarified that `probeWDL()` returns "pure" theoretical result (Fathom requires rule50=0)
- Clarified that `probeRoot()` uses actual halfmove clock for cursed win/blessed loss
- **Files:** `src/tablebase/Tablebase.h`, `src/tablebase/Tablebase.cpp`

**Fix 4: Test FEN Corrections**
- Fixed several illegal test positions where side-not-to-move was in check
- Added `EnPassantPosition` test to verify EP legality handling
- **File:** `test/tablebase/TablebaseTest.cpp`

**Confirmed Correct (No Fix Needed):**
- DTZ sign convention: Fathom returns absolute value (always positive), not signed
- Fathom's `tb_probe_wdl` REQUIRES rule50=0 (returns FAILED otherwise) - this is correct behavior


### Phase 5 Partial Completion Notes (2026-02-14)

**Implemented (merged into Phase 2):**
- UCI options: `SyzygyPath`, `SyzygyProbeDepth`, `SyzygyProbeRoot`
- YAML config support via ConfigRegistry
- Configuration validation at startup

**Total Estimated Time:** 2.5-3.5 weeks

---

## Risk Assessment

| Risk                   | Likelihood | Impact | Mitigation                                      |
|------------------------|------------|--------|-------------------------------------------------|
| Fathom API changes     | Low        | Medium | Pin to stable version                           |
| Thread safety issues   | Medium     | High   | Careful mutex design, test with multi-threading |
| Performance overhead   | Medium     | Medium | Probe depth limits, benchmarking                |
| Platform compatibility | Low        | Medium | Test on Windows and Linux                       |

---

## Success Criteria

1. **Functionality**
   - [x] TB files are loaded correctly on startup
   - [x] Root probing returns correct WDL and best move
   - [x] Search probing cuts off with known outcomes
   - [x] Graceful degradation when TB unavailable

2. **Performance**
   - [x] Average probe time < 100μs
   - [ ] No measurable NPS regression in non-TB positions (needs benchmark)
   - [x] Memory usage within configured limits

3. **Quality**
   - [x] All unit tests pass
   - [x] Integration tests with search pass
   - [ ] Manual testing with Arena/cutechess

4. **Strength**
   - [ ] +35-60 ELO in TB-relevant endgame suite (needs testing)
   - [ ] No regressions in non-endgame positions (needs testing)

---

## References

- [Syzygy Tablebases](https://syzygy-tables.info/) - Online probe and info
- [Fathom GitHub](https://github.com/jdart1/Fathom) - Probing library
- [Stockfish TB Code](https://github.com/official-stockfish/Stockfish/tree/master/src/syzygy) - Reference implementation
- [Chess Programming Wiki - Syzygy](https://www.chessprogramming.org/Syzygy_Endgame_Tablebases)

---

## Appendix A: Fathom API Reference

```c
// Key Fathom functions

// Initialize tablebases
// path: semicolon (Windows) or colon (Linux) separated list of directories
// Returns: true if at least one file was found
bool tb_init(const char *path);

// Free resources
void tb_free(void);

// Get largest tablebase available
// Returns: largest N such that N-piece tables are available
unsigned tb_largest(void);

// Probe WDL (Win/Draw/Loss)
// Returns: TB_WIN, TB_DRAW, TB_LOSS, TB_CURSED_WIN, TB_BLESSED_LOSS, or TB_RESULT_FAILED
unsigned tb_probe_wdl(
    uint64_t white, uint64_t black,
    uint64_t kings, uint64_t queens,
    uint64_t rooks, uint64_t bishops,
    uint64_t knights, uint64_t pawns,
    unsigned ep, bool turn);

// Probe root (includes DTZ and best move)
// Returns: TB_RESULT_FAILED or packed result (use TB_GET_* macros)
unsigned tb_probe_root(
    uint64_t white, uint64_t black,
    uint64_t kings, uint64_t queens,
    uint64_t rooks, uint64_t bishops,
    uint64_t knights, uint64_t pawns,
    unsigned rule50, unsigned ep, bool turn,
    unsigned *results);  // Output: array of moves+scores
```

---

## Appendix B: Test Positions

Standard TB test positions:

```
# KR vs K (White wins)
8/8/8/8/8/8/8/R3K2k w Q - 0 1

# KQ vs K (White wins)
8/8/8/8/8/8/8/KQ5k w - - 0 1

# KP vs K (Various outcomes based on pawn position)
8/P7/8/8/8/8/8/K6k w - - 0 1

# KBN vs K (White wins, but tricky)
8/8/8/8/8/8/8/KBN4k w - - 0 1

# KNN vs K (Draw)
8/8/8/8/8/8/8/KNN4k w - - 0 1

# KBBK vs K (White wins)
8/8/8/8/8/8/8/KBB4k w - - 0 1
```

---

*Last Updated: 2026-02-15*
