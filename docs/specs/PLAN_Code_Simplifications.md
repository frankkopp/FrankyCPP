# FrankyCPP — Code Simplification Plan (engine + chesscore)

**Document Version:** 1.0  
**Created:** 2026-04-11  
**Status:** 📋 PLANNING  
**Scope:** `src/engine/` and `src/chesscore/` — behavior-preserving simplifications only

---

## Goal

Reduce code duplication, verbosity, and unnecessary complexity across the engine and chesscore
modules. All changes must preserve identical behavior (same search results, same evaluation
scores). No algorithmic changes. The result should be easier to read, maintain, and extend.

---

## Guiding Principles

- **Behavior-preserving:** Identical search results before and after (bench signature unchanged).
- **Readability first:** Simplifications must make the code *easier* to understand, not harder.
- **No over-engineering:** Don't add abstraction layers that obscure intent (e.g., sub-struct
  grouping of flat data was rejected as adding complexity without real benefit).
- **One step at a time:** Each item is independently implementable and testable.
- **Verify with bench:** After each change, run `--bench --threads 1` and confirm signature matches.

---

## Summary Table

| #   | Item                                           | File(s)                    | Lines Saved | Effort     | Risk   |
|-----|------------------------------------------------|----------------------------|-------------|------------|--------|
| S1  | Deduplicate `evaluate()` / `evaluateTrace()`   | Evaluator.cpp, Evaluator.h | ~120        | 🟡 2–3 hrs | 🟢 Low |
| S2  | Loop over piece types in `pieceEval()` calls   | Evaluator.cpp              | ~4          | 🟢 15 min  | 🟢 Low |
| S3  | Extract TT bound-type stats helper             | Search.cpp                 | ~24         | 🟢 30 min  | 🟢 Low |
| S4  | Simplify `formatDetailedStats()` with helpers  | Search.cpp                 | ~80–100     | 🟡 1–2 hrs | 🟢 Low |
| S5  | Loop in `See::getLeastValuablePiece()`         | See.cpp                    | ~12         | 🟢 15 min  | 🟢 Low |
| S6  | Add `Score::addSigned()` helper                | score.h, Evaluator.cpp     | ~40         | 🟢 30 min  | 🟢 Low |
| S7  | Eliminate `if (mid \|\| end)` guard pattern    | Evaluator.cpp              | ~16         | 🟢 15 min  | 🟢 Low |
| S8  | Collapse king-safety safe-check blocks         | Evaluator.cpp              | ~15         | 🟢 30 min  | 🟢 Low |
| S9  | `SearchStats::operator+=` via field list macro | SearchStats.h              | ~50         | 🟡 1–2 hrs | 🟡 Med |
| S10 | Deduplicate TT-move validation pattern         | Search.cpp                 | ~8          | 🟢 15 min  | 🟢 Low |

**Estimated total:** ~370–390 lines removed/simplified

---

## Detailed Proposals

### S1: Deduplicate `evaluate()` / `evaluateTrace()`

**File:** `src/engine/Evaluator.cpp` (lines 63–356), `src/engine/Evaluator.h`  
**Problem:** `evaluate()` (lines 210–356) and `evaluateTrace()` (lines 63–208) share ~90%
identical code — reset state, material, positional, lazy eval, attack precompute, pawn eval,
piece eval, threats, coordination, king eval, tempo, final value. The only difference is that
`evaluateTrace()` records per-component score deltas into an `EvalTrace` struct.

**Solution:** Extract a shared `evaluateCore()` that takes an `EvalTrace*` parameter (nullable).
When non-null, record component deltas. Both public methods become thin wrappers:

```cpp
// Evaluator.h — add private method:
Value evaluateCore(const Position& p, EvalTrace* trace);

// Evaluator.cpp:
Value Evaluator::evaluate(const Position& p) {
  return evaluateCore(p, nullptr);
}

EvalTrace Evaluator::evaluateTrace(const Position& p) {
  EvalTrace trace{};
  evaluateCore(p, &trace);
  return trace;
}

Value Evaluator::evaluateCore(const Position& p, EvalTrace* trace) {
  // Single implementation — trace recording guarded by `if (trace)`
  // ...
}
```

**Lines saved:** ~120 (entire duplicate block eliminated)  
**Risk:** Low — logic is identical; trace-recording is additive.  
**Verification:** Bench signature + evaluateTrace unit tests must pass.

---

### S2: Loop over piece types in `pieceEval()` calls

**File:** `src/engine/Evaluator.cpp` (lines 306–314 in evaluate, 155–162 in evaluateTrace)  
**Problem:** Eight explicit `pieceEval()` calls — 4 piece types × 2 colors:

```cpp
pieceEval(p, score, WHITE, KNIGHT);
pieceEval(p, score, BLACK, KNIGHT);
pieceEval(p, score, WHITE, BISHOP);
// ... 5 more
```

**Solution:** Replace with a nested loop:

```cpp
for (const PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN}) {
  for (const Color c : Color::all()) {
    pieceEval(p, score, c, pt);
  }
}
```

**Lines saved:** ~4 per call site (8 lines → 4). If S1 is done first, only one call site remains.  
**Risk:** Low — iteration order change is irrelevant (each call is independent; score accumulates additively).  
**Note:** Order within `pieceEval()` is COLOR-then-TYPE now (W knight, B knight, W bishop...).
The loop preserves this. If TYPE-then-COLOR were needed, swap the loop nesting.

---

### S3: Extract TT bound-type stats helper

**File:** `src/engine/Search.cpp` (lines 1373–1386, 2155–2168, 2771–2784)  
**Problem:** The identical `switch (ttEntry->type)` block for tracking `ttHitNone/Exact/Alpha/Beta`
appears three times: in `search()`, `qsearch()`, and `extractPonderMove()`.

```cpp
switch (ttEntry->type) {
  case NONE:  STAT_INC(thread().statistics.ttHitNone);  break;
  case EXACT: STAT_INC(thread().statistics.ttHitExact); break;
  case ALPHA: STAT_INC(thread().statistics.ttHitAlpha); break;
  case BETA:  STAT_INC(thread().statistics.ttHitBeta);  break;
}
```

**Solution:** Add a private inline helper to `Search` (or a free function in an anonymous namespace):

```cpp
inline void trackTtBoundType(SearchStats& stats, const ValueType type) {
  switch (type) {
    case NONE:  STAT_INC(stats.ttHitNone);  break;
    case EXACT: STAT_INC(stats.ttHitExact); break;
    case ALPHA: STAT_INC(stats.ttHitAlpha); break;
    case BETA:  STAT_INC(stats.ttHitBeta);  break;
  }
}
```

Replace each 6-line switch with a single call:  
`trackTtBoundType(thread().statistics, ttEntry->type);`

**Lines saved:** ~24 (3 × 6 lines → 3 × 1 line + 6-line helper)  
**Risk:** Low — pure stat tracking, no behavioral change.

---

### S4: Simplify `formatDetailedStats()` with helpers

**File:** `src/engine/Search.cpp` (lines 3567–3837)  
**Problem:** `formatDetailedStats()` is ~270 lines of repetitive formatting. Many blocks follow
identical patterns:

1. **"value with percentage"** — appears ~15 times:
   ```cpp
   os << "PV Nodes       : " << stats.pvNodes;
   if (totalNodes > 0) {
     const double pvPct = 100.0 * static_cast<double>(stats.pvNodes) / static_cast<double>(totalNodes);
     os << " (" << std::fixed << std::setprecision(2) << pvPct << "%)";
   }
   os << "\n";
   ```

2. **"label : value"** — appears ~30 times:
   ```cpp
   os << "Beta Cuts      : " << stats.betaCuts << "\n";
   ```

**Solution:** Add two local lambdas at the top of the function:

```cpp
// Emit "Label : value (XX.X%)\n"
const auto pctLine = [&](const char* label, const uint64_t val, const uint64_t total, const int prec = 1) {
  os << label << val;
  if (total > 0) {
    os << " (" << std::fixed << std::setprecision(prec)
       << (100.0 * static_cast<double>(val) / static_cast<double>(total)) << "%)";
  }
  os << "\n";
};

// Emit "Label : value\n"
const auto line = [&](const char* label, const auto& val) {
  os << label << val << "\n";
};
```

Each 5-line block becomes a single `pctLine(...)` call. Each 1-line value becomes `line(...)`.

**Lines saved:** ~80–100 (rough estimate; the function shrinks from ~270 to ~170 lines)  
**Risk:** Low — output format unchanged; pure display code.

---

### S5: Loop in `See::getLeastValuablePiece()`

**File:** `src/engine/See.cpp` (lines 86–107)  
**Problem:** Six sequential if-blocks checking each piece type in value order:

```cpp
if ((bitboard & p.getPieceBb(color, PAWN)) != 0)
  return (bitboard & p.getPieceBb(color, PAWN)).lsb();
if ((bitboard & p.getPieceBb(color, KNIGHT)) != 0)
  return (bitboard & p.getPieceBb(color, KNIGHT)).lsb();
// ... BISHOP, ROOK, QUEEN, KING
```

**Solution:** Replace with a loop over a value-ordered array:

```cpp
Square See::getLeastValuablePiece(const Position& p, const Bitboard bitboard, const Color color) {
  static constexpr PieceType order[] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};
  for (const PieceType pt : order) {
    const Bitboard match = bitboard & p.getPieceBb(color, pt);
    if (match) return match.lsb();
  }
  return SQ_NONE;
}
```

**Lines saved:** ~12 (18 lines → 6)  
**Risk:** Low — identical behavior; loop iterates in exact same order.  
**Performance note:** This is not hot-path critical (called a few dozen times per SEE).
The loop may even be faster due to better branch prediction on modern CPUs.

---

### S6: Add `Score::addSigned()` helper to eliminate repeated pattern

**File:** `src/types/score.h`, `src/engine/Evaluator.cpp`  
**Problem:** The pattern `s.midgame += static_cast<Value>(mid * us.sign()); s.endgame += static_cast<Value>(end * us.sign());`
appears **7 times** in Evaluator.cpp (knightEval, bishopEval, rookEval, queenEval, kingEval,
threatEval, coordinationEval), plus 4 instances of `if (mid || end) { ... same pattern ... }`.

Each instance is 2–4 lines of verbose casting:
```cpp
if (mid || end) {
  s.midgame += static_cast<Value>(mid * us.sign());
  s.endgame += static_cast<Value>(end * us.sign());
}
```

**Solution:** Add a `Score::addSigned()` free function to `score.h`:

```cpp
/// Add mid/end values signed by color (positive for WHITE, negated for BLACK).
/// Common pattern in evaluation: accumulate per-color bonuses into a white-relative score.
constexpr void addSigned(Score& s, const int mid, const int end, const int sign) {
  s.midgame += static_cast<Value>(mid * sign);
  s.endgame += static_cast<Value>(end * sign);
}
```

Each call site becomes:
```cpp
addSigned(s, mid, end, us.sign());
```

**Lines saved:** ~40 across all call sites (7 × 2-line blocks + 4 × 4-line guarded blocks → 11 × 1-line calls)  
**Risk:** Low — trivially equivalent.  
**Note:** The `if (mid || end)` guard is a premature optimization (the additions are cheaper than
the branch). Removing it is safe and actually improves branch prediction. If desired, the guard
can be kept by adding an `if (mid || end)` check before the call.

---

### S7: Eliminate `if (mid || end)` guard pattern in eval functions

**File:** `src/engine/Evaluator.cpp` (lines 754, 796, 992, 1041)  
**Problem:** Four eval functions (`rookEval`, `queenEval`, `threatEval`, `coordinationEval`)
guard their final score accumulation with `if (mid || end)`:

```cpp
if (mid || end) {
  s.midgame += static_cast<Value>(mid * us.sign());
  s.endgame += static_cast<Value>(end * us.sign());
}
```

This is a premature optimization — two additions with zero are cheaper than a branch + two
comparisons. The guard adds 2 extra lines per function for no measurable benefit.

**Solution:** Remove the `if (mid || end)` guard; always accumulate. This is safe because
adding zero is a no-op:

```cpp
s.midgame += static_cast<Value>(mid * us.sign());
s.endgame += static_cast<Value>(end * us.sign());
```

Combined with S6, each becomes a single call: `addSigned(s, mid, end, us.sign());`

**Lines saved:** ~16 (4 × 4 lines → 4 × 1 or 2 lines)  
**Risk:** Low — adding zero is identical to not adding.  
**Performance:** Removing a branch is actually marginally better for modern CPUs with deep pipelines.

---

### S8: Collapse king-safety safe-check blocks

**File:** `src/engine/Evaluator.cpp` (lines 911–938, inside `kingEval()`)  
**Problem:** Three near-identical blocks check safe checks for knight, rook, and queen:

```cpp
if (p.getPieceBb(them, KNIGHT)) {
  const Bitboard checkSquares = Bitboards::nonSliderAttacks[KNIGHT][ksq];
  const int safeChecks        = (checkSquares & safeMask).popcount();
  mid += safeChecks * EvalConfig.SAFE_CHECK_KNIGHT_MID;
}
if (p.getPieceBb(them, ROOK)) {
  const Bitboard checkSquares = Attacks::attacks(ROOK, ksq, occupied);
  const int safeChecks        = (checkSquares & safeMask).popcount();
  mid += safeChecks * EvalConfig.SAFE_CHECK_ROOK_MID;
}
if (p.getPieceBb(them, QUEEN)) {
  const Bitboard checkSquares = Attacks::attacks(QUEEN, ksq, occupied);
  const int safeChecks        = (checkSquares & safeMask).popcount();
  mid += safeChecks * EvalConfig.SAFE_CHECK_QUEEN_MID;
}
```

**Solution:** Use a data-driven approach with a small struct array:

```cpp
struct SafeCheckEntry { PieceType pt; int weight; };
static constexpr SafeCheckEntry safeChecks[] = {
  {KNIGHT, 0}, // weight filled from config at call time
  {ROOK,   0},
  {QUEEN,  0},
};
```

Or simply extract an inline lambda since the weights come from config:

```cpp
const auto countSafeChecks = [&](const PieceType pt, const int weight) {
  if (p.getPieceBb(them, pt)) {
    const Bitboard checkSq = (pt == KNIGHT)
      ? Bitboards::nonSliderAttacks[KNIGHT][ksq]
      : Attacks::attacks(pt, ksq, occupied);
    mid += (checkSq & safeMask).popcount() * weight;
  }
};
countSafeChecks(KNIGHT, EvalConfig.SAFE_CHECK_KNIGHT_MID);
countSafeChecks(ROOK,   EvalConfig.SAFE_CHECK_ROOK_MID);
countSafeChecks(QUEEN,  EvalConfig.SAFE_CHECK_QUEEN_MID);
```

**Lines saved:** ~15 (18 lines → 3 calls + 6-line lambda)  
**Risk:** Low — identical logic, just factored.  
**Note:** The knight case uses `nonSliderAttacks` while rook/queen use `Attacks::attacks()`.
The lambda handles this with a single ternary.

---

### S9: `SearchStats::operator+=` via X-macro or field list

**File:** `src/engine/SearchStats.h` (lines 325–415)  
**Problem:** `operator+=` is 85 lines of `field += other.field;` for 50+ fields. Every time a
new stat field is added, the developer must remember to add it to `operator+=` (and `operator<<`).
Forgetting causes silent data loss in SMP stats aggregation.

**Solution A — X-macro (recommended):**  
Define a macro list of all cumulative fields. Use it to generate both the field declarations
and the `operator+=` body:

```cpp
// In SearchStats.h:
#define SEARCHSTATS_CUMULATIVE_FIELDS(X) \
  X(checkmates)                          \
  X(stalemates)                          \
  X(perftNodeCount)                      \
  X(pvNodes)                             \
  X(nonPvNodes)                          \
  /* ... all 50+ fields ... */

SearchStats& operator+=(const SearchStats& other) {
  #define ADD_FIELD(name) name += other.name;
  SEARCHSTATS_CUMULATIVE_FIELDS(ADD_FIELD)
  #undef ADD_FIELD
  // Handle betaCutsByIndex array separately
  for (int i = 0; i < BETA_CUTS_INDEX_SIZE; ++i) {
    betaCutsByIndex[i] += other.betaCutsByIndex[i];
  }
  return *this;
}
```

**Solution B — Reflection-style helper (simpler, less macro):**  
Use `std::apply` with a tuple of member pointers. More C++20-idiomatic but more complex setup.

**Lines saved:** ~50 (85-line operator → ~35 lines including macro definition)  
**Risk:** Medium — X-macros are a well-known pattern but reduce readability of field declarations.
If rejected, keep the current flat approach (it works, it's just verbose).  
**Alternative:** If X-macro is too invasive, simply keep the status quo and ensure a code comment
at the top of `operator+=` says "IMPORTANT: add new fields here when adding stats".

---

### S10: Deduplicate TT-move validation pattern

**File:** `src/engine/Search.cpp` (lines 1359–1364, 2145–2149, 2785–2786)  
**Problem:** The TT-move validation pattern appears 3 times:

```cpp
const auto probedMove = static_cast<Move>(ttEntry->move);
if (probedMove != MOVE_NONE && MoveGenerator::isPseudoLegal(p, probedMove)) {
  ttMove = probedMove;
}
```

**Solution:** Extract an inline helper:

```cpp
/// Validate a TT move: returns the move if pseudo-legal, MOVE_NONE otherwise.
[[nodiscard]] inline Move validateTtMove(const Position& p, const uint16_t rawMove) {
  const auto move = static_cast<Move>(rawMove);
  if (move != MOVE_NONE && MoveGenerator::isPseudoLegal(p, move)) {
    return move;
  }
  return MOVE_NONE;
}
```

Each call site becomes: `ttMove = validateTtMove(p, ttEntry->move);`

**Lines saved:** ~8 (3 × 3 lines → 3 × 1 line + 5-line helper)  
**Risk:** Low — identical validation logic.

---

## Implementation Order

Recommended order (dependencies and risk-first):

1. **S5** — `See::getLeastValuablePiece` loop (isolated, trivial, good warm-up)
2. **S3** — TT bound-type stats helper (isolated, trivial)
3. **S10** — TT-move validation helper (isolated, trivial)
4. **S6** — `addSigned()` helper in `score.h` (foundation for S7)
5. **S7** — Remove `if (mid || end)` guards (depends on S6)
6. **S8** — Safe-check lambda collapse (independent)
7. **S2** — pieceEval loop (small, independent)
8. **S1** — Evaluator dedup (largest change, highest value — do after warm-up)
9. **S4** — `formatDetailedStats()` helpers (large but low-risk display code)
10. **S9** — SearchStats X-macro (optional — highest risk, discuss before implementing)

---

## Verification Checklist

For each item:
- [ ] Bench signature unchanged (`--bench --threads 1`)
- [ ] All unit tests pass (excluding speed/timing tests)
- [ ] No new compiler warnings
- [ ] Code review: readability improved, not degraded

---

## Items Considered and Rejected

| Item                                   | Reason for Rejection                                                                                                                                                                                                                        |
|----------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Sub-struct grouping of SearchStats** | One flat struct is simpler than 5 sub-structs. Total line count barely changes; adds indirection (`stats.tt.hits` vs `stats.ttHits`) without real benefit.                                                                                  |
| **Template-based Evaluator**           | Templating `evaluate<bool Trace>` would eliminate the runtime `if (trace)` checks in S1, but adds template complexity and longer compile times for minimal gain. The `if (trace)` branches are trivially predicted and not in the hot path. |

---

*Last updated: 2026-04-11*
