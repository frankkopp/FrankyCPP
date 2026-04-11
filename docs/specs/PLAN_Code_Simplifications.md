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

## Verification of Existing Findings (S1–S10)

All 10 original findings were reviewed against the current codebase on 2026-04-11.
Every finding remains valid and actionable. No code has changed that would invalidate
any proposal. Line numbers are still accurate.

---

## Additional Findings (S11–S25)

*Added: 2026-04-11 — Focus: simplifications without sacrificing performance + performance improvements*
*Scope: `src/engine/`, `src/chesscore/`, `src/tablebase/`, `src/types/`*

### Updated Summary Table (S11–S25)

| #   | Item                                                                     | File(s)       | Category      | Severity | Confidence | Effort     | Risk   |
|-----|--------------------------------------------------------------------------|---------------|---------------|----------|------------|------------|--------|
| S11 | ✅ Bulk pawn attack computation (shift vs loop)                           | Evaluator.cpp | PERFORMANCE   | CRITICAL | HIGH       | 🟡 1 hr    | 🟢 Low |
| S12 | King safety attack accumulation per-piece dedup                          | Evaluator.cpp | REDUNDANCY    | MEDIUM   | CERTAIN    | 🟢 30 min  | 🟢 Low |
| S13 | `return value` instead of `return (value)` in qsearch drop               | Search.cpp    | READABILITY   | LOW      | CERTAIN    | 🟢 5 min   | 🟢 Low |
| S14 | Redundant `const auto do_null` in move loop                              | Search.cpp    | READABILITY   | LOW      | CERTAIN    | 🟢 5 min   | 🟢 Low |
| S15 | Extract `checkDrawRepAnd50` + draw score pattern                         | Search.cpp    | REDUNDANCY    | MEDIUM   | HIGH       | 🟢 15 min  | 🟢 Low |
| S16 | `formatDetailedStats` — percentage lambda (stronger version of S4)       | Search.cpp    | REDUNDANCY    | MEDIUM   | CERTAIN    | 🟡 1–2 hrs | 🟢 Low |
| S17 | TT `aggregateStats()` duplication with `aggregateInstrumentationStats()` | TT.h          | REDUNDANCY    | LOW      | CERTAIN    | 🟢 30 min  | 🟢 Low |
| S18 | TT getter methods call `aggregateStats()` individually                   | TT.h          | PERFORMANCE   | MEDIUM   | HIGH       | 🟢 30 min  | 🟢 Low |
| S19 | History decay: clamp to 0 on fail-low                                    | Search.cpp    | CHESS_PATTERN | HIGH     | MEDIUM     | 🟢 15 min  | 🟡 Med |
| S20 | `passedPawns[]` fallback duplicated across evaluate/evaluateTrace        | Evaluator.cpp | REDUNDANCY    | MEDIUM   | CERTAIN    | 🟢 15 min  | 🟢 Low |
| S21 | Move loop beta-cut stats block duplicated search/qsearch                 | Search.cpp    | REDUNDANCY    | MEDIUM   | CERTAIN    | 🟢 30 min  | 🟢 Low |
| S22 | `EVAL_PREFETCH` inconsistency between search and qsearch                 | Search.cpp    | PERFORMANCE   | HIGH     | HIGH       | 🟢 5 min   | 🟢 Low |
| S23 | `pieceEval` switch default unreachable                                   | Evaluator.cpp | DEAD_CODE     | LOW      | CERTAIN    | 🟢 5 min   | 🟢 Low |
| S24 | `relRank` computation duplicated across eval functions                   | Evaluator.cpp | REDUNDANCY    | LOW      | CERTAIN    | 🟢 30 min  | 🟢 Low |
| S25 | `Evaluator::reset()` is empty — remove or document                       | Evaluator.h   | DEAD_CODE     | LOW      | CERTAIN    | 🟢 5 min   | 🟢 Low |

---

### S11: ✅ Bulk pawn attack computation via shift instead of per-square loop

**Status:** ✅ COMPLETE — Used `Bitboard::shifted()` (matching MoveGenerator style) instead of raw shifts.
**Confidence:** HIGH

**Problem:** Pawn attacks are computed by looping over each pawn individually and OR-ing
`pawnAttacks[color][sq]` one square at a time. For a typical position with 8 pawns per side,
that's 16 loop iterations with `popLSB()` + table lookup + OR.

The standard technique in chess engines (used by Stockfish and virtually every strong engine)
is to compute all pawn attacks at once using bitboard shifts:

```cpp
// Current: loop per pawn
Bitboard wp = p.getPieceBb(WHITE, PAWN);
while (wp) { attackedByPT[PAWN][WHITE] |= Bitboards::pawnAttacks[WHITE][wp.popLSB()]; }
```

**Solution:** Replace with two shift operations:

```cpp
// White pawn attacks: shift NE and NW
const Bitboard wp = p.getPieceBb(WHITE, PAWN);
attackedByPT[PAWN][WHITE] = ((wp & ~FileHBB) << 9) | ((wp & ~FileABB) << 7);
attackedBy[WHITE] |= attackedByPT[PAWN][WHITE];

// Black pawn attacks: shift SE and SW
const Bitboard bp = p.getPieceBb(BLACK, PAWN);
attackedByPT[PAWN][BLACK] = ((bp & ~FileABB) >> 9) | ((bp & ~FileHBB) >> 7);
attackedBy[BLACK] |= attackedByPT[PAWN][BLACK];
```

**Lines saved:** ~6 per call site (two call sites if S1 is not done first)
**Risk:** None — behavioral equivalent. The shift approach computes the exact same bitboard.
**Performance note:** This eliminates ~16 iterations of `popLSB()` + table lookup per `evaluate()` call.
Two shift + mask operations replace a variable-length loop. On modern x86, each shift+AND is
1 cycle; the loop version involves branch prediction, table cache misses, and serial dependency
on `popLSB()`. This is called millions of times per search — the cumulative savings are meaningful.

**Note:** Verify that `FileABB`, `FileHBB` constants exist (they should in `bitboards.h`). If the
Bitboard class doesn't support `<<`/`>>` with edge masking directly, use the raw shift on the
underlying `uint64_t` via `.value()` or implicit conversion.

---

### S12: King safety attack accumulation — deduplicate per-piece pattern

**File:** `src/engine/Evaluator.cpp` (in `knightEval`, `bishopEval`, `rookEval`, `queenEval`)
**Category:** REDUNDANCY
**Severity:** MEDIUM
**Confidence:** CERTAIN

**Problem:** The identical king safety attack accumulation block appears in all four piece eval functions:

```cpp
if (EvalConfig.USE_KING_SAFETY_ATTACK) {
  const Color them             = ~us;
  const Bitboard enemyKingZone = Bitboards::nonSliderAttacks[KING][p.getKingSquare(them)];
  if (attacks & enemyKingZone) {
    ++kingAttackCount[them];
    kingAttackWeight[them] += EvalConfig.KING_ATTACK_WEIGHT_KNIGHT; // varies by piece
  }
}
```

The only difference is the weight constant (`KING_ATTACK_WEIGHT_KNIGHT/BISHOP/ROOK/QUEEN`).

**Solution:** Extract an inline helper:

```cpp
inline void Evaluator::accumulateKingAttack(const Bitboard attacks, const Color us, const int weight) {
  if (EvalConfig.USE_KING_SAFETY_ATTACK) {
    const Color them = ~us;
    const Bitboard enemyKingZone = Bitboards::nonSliderAttacks[KING][p.getKingSquare(them)];
    if (attacks & enemyKingZone) {
      ++kingAttackCount[them];
      kingAttackWeight[them] += weight;
    }
  }
}
```

However, this would need `p` as a parameter too. Alternative: since the `enemyKingZone` is
the same for all pieces of a given color, precompute it once in `evaluate()` and store in a
member variable, then use a simpler inline:

```cpp
// In evaluate(), after king attack pre-computation:
enemyKingZone[WHITE] = Bitboards::nonSliderAttacks[KING][p.getKingSquare(WHITE)];
enemyKingZone[BLACK] = Bitboards::nonSliderAttacks[KING][p.getKingSquare(BLACK)];

// In each piece eval (replaces 5-line block):
if (EvalConfig.USE_KING_SAFETY_ATTACK && (attacks & enemyKingZone[them])) {
  ++kingAttackCount[them];
  kingAttackWeight[them] += weight;
}
```

**Lines saved:** ~16 (4 × 5-line blocks → 4 × 2-line blocks + 2-line precompute)
**Risk:** None — behavioral equivalent.
**Performance note:** Precomputing `enemyKingZone` once avoids 8 redundant table lookups
(`nonSliderAttacks[KING][sq]`) per `evaluate()` call. Tiny but positive.

---

### S13: Unnecessary intermediate variable in depth-0 qsearch drop

**File:** `src/engine/Search.cpp` (lines 1301–1304)
**Category:** READABILITY
**Severity:** LOW
**Confidence:** CERTAIN

**Problem:**
```cpp
if (depth == 0 || ply >= MAX_DEPTH) {
  const auto value = qsearch(p, ply, alpha, beta, nodeType);
  return value;
}
```

**Solution:**
```cpp
if (depth == 0 || ply >= MAX_DEPTH) {
  return qsearch(p, ply, alpha, beta, nodeType);
}
```

**Lines saved:** 1
**Risk:** None — behavioral equivalent.

---

### S14: `matethreat` check for `do_null` computed identically twice

**File:** `src/engine/Search.cpp` (line 1952 in search move loop, line 1638 in NMP verify)
**Category:** READABILITY
**Severity:** LOW
**Confidence:** CERTAIN

**Problem:** `const auto do_null = matethreat ? No_Null_Move : Do_Null_Move;` appears at line 1952
inside the move loop but `matethreat` is constant by that point. The same ternary also appears
at line 1638 for NMP verification. Within the move loop, this is recomputed every iteration
for a value that never changes.

**Solution:** Move `do_null` computation before the move loop (after `matethreat` is finalized
by null-move search):

```cpp
// After NMP section, before move loop:
const auto do_null = matethreat ? No_Null_Move : Do_Null_Move;
```

**Lines saved:** 1 (removes per-iteration recomputation)
**Risk:** None — behavioral equivalent. `matethreat` is only set in the NMP block and never
changed afterward.
**Performance note:** Eliminates a branch inside the hot move loop. Tiny but correct.

---

### S15: Extract draw-check-and-score pattern (search + qsearch)

**File:** `src/engine/Search.cpp` (lines 1949 and 2298)
**Category:** REDUNDANCY
**Severity:** MEDIUM
**Confidence:** HIGH

**Problem:** The pattern `if (checkDrawRepAnd50(p, 2)) { value = drawScore(p); }` followed
by `else { /* recursive search */ }` appears identically in both `search()` and `qsearch()`.

This is a minor redundancy — only 2 lines — but it's part of a larger pattern where the
post-move handling (draw check → search → undo → bestValue update → beta cut) is
structurally identical between the two functions. Extracting the draw check itself isn't
worthwhile alone, but it becomes valuable if combined with S21.

**Solution:** No standalone change recommended. Flag for consideration if S21 is implemented.

**Lines saved:** 0 standalone (part of S21)
**Risk:** None.

---

### S16: `formatDetailedStats()` — comprehensive helper extraction (supersedes S4)

**File:** `src/engine/Search.cpp` (lines 3567–3837)
**Category:** REDUNDANCY
**Severity:** MEDIUM
**Confidence:** CERTAIN

**Problem:** This function is 270 lines with three repeating patterns:

1. **Value with percentage** (~15 instances): `os << label << value; if (total > 0) { os << " (" << pct << "%)"; } os << "\n";`
2. **Plain label:value** (~30 instances): `os << label << value << "\n";`
3. **Conditional section with percentage breakdown** (~5 instances): similar to #1 but with
   a guarded `if (total > 0)` section containing multiple sub-lines.

S4 already proposed lambda helpers for patterns 1 and 2. This finding extends S4 with
a third helper for the section pattern and notes that the function can shrink to ~100 lines.

**Solution:** Three local lambdas at the top:

```cpp
const auto line = [&](const char* label, const auto& val) {
  os << label << val << "\n";
};

const auto pctLine = [&](const char* label, const uint64_t val, const uint64_t total, const int prec = 1) {
  os << label << val;
  if (total > 0) {
    os << " (" << std::fixed << std::setprecision(prec)
       << (100.0 * static_cast<double>(val) / static_cast<double>(total)) << "%)";
  }
  os << "\n";
};

const auto pctLineFmt = [&](const char* label, const uint64_t val, const uint64_t total,
                            const char* suffix = "", const int prec = 1) {
  os << label << val;
  if (total > 0) {
    os << " (" << std::fixed << std::setprecision(prec)
       << (100.0 * static_cast<double>(val) / static_cast<double>(total)) << "%)";
  }
  if (suffix[0]) os << " " << suffix;
  os << "\n";
};
```

**Lines saved:** ~120–150 (270 → ~120 lines)
**Risk:** None — pure display code, no behavioral change.
**Note:** Supersedes S4 — implement this instead of S4.

---

### S17: TT `aggregateStats()` and `aggregateInstrumentationStats()` pattern duplication

**File:** `src/engine/TT.h` (both aggregate functions)
**Category:** REDUNDANCY
**Severity:** LOW
**Confidence:** CERTAIN

**Problem:** Both functions have the same loop structure iterating `numSmpThreads` slots
and summing fields. The pattern is identical but operates on different struct types.

**Solution:** A generic template helper:

```cpp
template<typename T, std::size_t N>
[[nodiscard]] T aggregateSlots(const std::array<T, N>& slots, const int count) const {
  T total{};
  for (int i = 0; i < count; ++i) { /* field-wise add */ }
  return total;
}
```

However, without a generic `operator+=` on both structs, this doesn't save much.
Alternative: add `operator+=` to both `Stats` and `InstrumentationStats`, then the
aggregation loop becomes a 3-line function. This mirrors the S9 pattern.

**Lines saved:** ~15
**Risk:** None — cold path only.

---

### S18: TT getter methods each call `aggregateStats()` independently

**File:** `src/engine/TT.h` (getNumberOfEntries, getNumberOfPuts, etc.)
**Category:** PERFORMANCE
**Severity:** MEDIUM
**Confidence:** HIGH

**Problem:** Each public getter (`getNumberOfEntries()`, `getNumberOfPuts()`,
`getNumberOfCollisions()`, etc.) independently calls `aggregateStats()`, which loops
over all thread slots. In `formatDetailedStats()`, multiple getters are called in
sequence, causing redundant aggregation loops.

**Solution:** The callers (e.g., `formatDetailedStats`) should call `aggregateStats()` once
and access fields directly. The getters are fine for single-field access but should not
be used in batch. Add a public `getAggregatedStats()` method or make `aggregateStats()` public:

```cpp
public:
  [[nodiscard]] Stats getAggregatedStats() const { return aggregateStats(); }
```

Then in `formatDetailedStats`, call once and use the struct.

**Lines saved:** ~10 in the caller, plus avoids 7× redundant aggregation loops.
**Risk:** None — cold path, behavioral equivalent.
**Performance note:** Not hot path, but `formatDetailedStats` is called from UCI and logging.
Avoids O(7 × numThreads) redundant iterations.

---

### S19: History table decay — clamping to 0 on fail-low is lossy

**File:** `src/engine/Search.cpp` (lines 2070–2073)
**Category:** CHESS_PATTERN
**Severity:** HIGH
**Confidence:** MEDIUM

**Problem:** When a quiet move fails to improve alpha, its history count is decremented
and then clamped to 0:

```cpp
thread().history.historyCount[us][from][to] -= 1L << depth;
if (thread().history.historyCount[us][from][to] < 0) {
  thread().history.historyCount[us][from][to] = 0;
}
```

The clamp to 0 means negative information is lost — a move that consistently fails is
indistinguishable from a move never tried. Most modern engines (Stockfish, Ethereal)
allow negative history values. This enables LMR to reduce these moves *more* aggressively.

**Solution:** Remove the clamp-to-zero:

```cpp
thread().history.historyCount[us][from][to] -= 1L << depth;
```

Optionally add a floor (e.g., `-16384`) to prevent overflow in extremely long searches,
and apply gravity/aging to prevent unbounded growth. The LMR history adjustment
(lines 1886–1894) already reads this value and adjusts reduction accordingly — negative
values would naturally increase reduction for bad moves.

**Lines saved:** 3
**Risk:** MEDIUM — this **changes search behavior**. The bench signature will change.
Requires Elo testing (e.g., SPRT test at 10+0.1, ~2000 games). Expected to be Elo-positive
based on widespread adoption in other engines, but must be verified.
**Note:** This is flagged as a chess-engine-specific opportunity, not a simplification.

---

### S20: `passedPawns[]` fallback computation duplicated in evaluate/evaluateTrace

**File:** `src/engine/Evaluator.cpp` (evaluate ~line 273, evaluateTrace ~line 143)
**Category:** REDUNDANCY
**Severity:** MEDIUM
**Confidence:** CERTAIN

**Problem:** The `else` branch that computes `passedPawns[]` when `USE_PAWN_EVAL` is disabled
is duplicated verbatim between `evaluate()` and `evaluateTrace()`. Each copy is ~12 lines.

**Solution:** If S1 is implemented first, this goes away automatically (single `evaluateCore()`).
If S1 is deferred, extract a private helper:

```cpp
void Evaluator::computePassedPawns(const Position& p) {
  for (const Color c : Color::all()) {
    // ... existing logic ...
  }
}
```

**Lines saved:** ~12 (if standalone; 0 if S1 is done first)
**Risk:** None — behavioral equivalent.
**Note:** This is a sub-finding of S1. Implement S1 to resolve both.

---

### S21: Beta-cut statistics block duplicated between search and qsearch

**File:** `src/engine/Search.cpp` (search ~lines 2034–2054, qsearch ~lines 2318–2328)
**Category:** REDUNDANCY
**Severity:** MEDIUM
**Confidence:** CERTAIN

**Problem:** The beta cutoff handling — statistics tracking, killer move storage, history
update, counter move storage — follows the same pattern in both `search()` and `qsearch()`.
The qsearch version is a subset (no killer/history/counter updates, just stats), but the
statistics portion is identical:

```cpp
STAT_INC(thread().statistics.betaCuts);
STAT_INC(thread().statistics.betaCutsByIndex[std::min(movesSearched - 1, SearchStats::BETA_CUTS_INDEX_SIZE - 1)]);
if (movesSearched == 1 && ttMove != MOVE_NONE && move == ttMove) {
  STAT_INC(thread().statistics.ttMoveBestMove);
}
```

**Solution:** Extract an inline helper:

```cpp
inline void Search::recordBetaCut(const Move move, const Move ttMove, const int movesSearched) {
  STAT_INC(thread().statistics.betaCuts);
  STAT_INC(thread().statistics.betaCutsByIndex[std::min(movesSearched - 1, SearchStats::BETA_CUTS_INDEX_SIZE - 1)]);
  if (movesSearched == 1 && ttMove != MOVE_NONE && move == ttMove) {
    STAT_INC(thread().statistics.ttMoveBestMove);
  }
}
```

**Lines saved:** ~8 (2 × 5-line blocks → 2 × 1-line calls + 5-line helper)
**Risk:** None — pure statistics tracking.

---

### S22: `EVAL_PREFETCH` commented out in search but active in qsearch

**File:** `src/engine/Search.cpp` (line 1940 vs line 2290)
**Category:** PERFORMANCE
**Severity:** HIGH
**Confidence:** HIGH

**Problem:** In `search()` at line 1940, `EVAL_PREFETCH` is commented out:
```cpp
TT_PREFETCH;
// EVAL_PREFETCH;
```

But in `qsearch()` at line 2290, it's active:
```cpp
TT_PREFETCH;
EVAL_PREFETCH;
```

This is inconsistent. If pawn TT prefetching is beneficial (which it should be — hiding
memory latency before `evaluate()` is called), it should be active in both. If it was
disabled in `search()` for a reason, it should also be disabled in `qsearch()`, or the
reason should be documented.

**Solution:** Either uncomment in `search()` or add a comment explaining why it's disabled.
Recommended: enable in both, since the `evaluate()` call path is identical.

**Lines saved:** 0
**Risk:** Low — prefetch is a performance hint with no behavioral effect. The worst case
is a wasted cache line load if the position takes a TT cut before reaching evaluate().
**Performance note:** Pawn TT prefetch gives the memory subsystem ~100-300 cycles of lead
time. In `search()`, there's substantial work between doMove and evaluate (legality check,
draw check, recursive search), so the prefetch would be fully absorbed.

---

### S23: `pieceEval` switch — unreachable default branch

**File:** `src/engine/Evaluator.cpp` (inside `pieceEval()`)
**Category:** DEAD_CODE
**Severity:** LOW
**Confidence:** CERTAIN

**Problem:** The `switch (pieceType)` in `pieceEval()` has a `default: break;` case.
Since `pieceEval` is only called with `KNIGHT`, `BISHOP`, `ROOK`, `QUEEN` (from the
explicit call sites in `evaluate()`), the default branch is unreachable.

**Solution:** Replace `default: break;` with `default: __assume(false);` (MSVC) or
`__builtin_unreachable()` (GCC/Clang) to help the optimizer, or simply remove the
default case. If S2 (loop over piece types) is implemented, the loop already constrains
the values.

Alternatively, add `assert(false && "pieceEval called with unexpected piece type");`
for debug builds.

**Lines saved:** 2
**Risk:** None — dead code removal.

---

### S24: `relRank` computation duplicated across multiple eval functions

**File:** `src/engine/Evaluator.cpp` (knightEval, rookEval, pawnEval, kingEval)
**Category:** REDUNDANCY
**Severity:** LOW
**Confidence:** CERTAIN

**Problem:** The pattern `const int relRank = us == WHITE ? static_cast<int>(sq.rank()) : 7 - static_cast<int>(sq.rank());`
appears in at least 4 different eval functions.

**Solution:** Add a `relativeRank(Color c, Square sq)` free function or method:

```cpp
constexpr int relativeRank(const Color c, const Square sq) {
  return c == WHITE ? static_cast<int>(sq.rank()) : 7 - static_cast<int>(sq.rank());
}
```

This could go in `square.h` or `color.h` as a utility. Each call site becomes
`const int relRank = relativeRank(us, sq);`.

**Lines saved:** ~4 (shorter lines at 4+ call sites)
**Risk:** None — behavioral equivalent.
**Note:** Check if `Square` already has a `relativeRank(Color)` method. If not, adding one
to `Square` would be the most natural API.

---

### S25: `Evaluator::reset()` is empty — remove or document

**File:** `src/engine/Evaluator.h` (lines ~170–174)
**Category:** DEAD_CODE
**Severity:** LOW
**Confidence:** CERTAIN

**Problem:**
```cpp
void reset() {
  // Nothing to reset - scratch variables are reset per-call
}
```

This method does nothing. It exists with a ReSharper suppression comment and a body
that is a no-op. Callers calling `reset()` are doing busywork.

**Solution:** Either remove the method entirely and remove any call sites, or mark it
`[[deprecated("scratch variables are reset per-call")]]` to catch future callers.
If it's kept as a "lifecycle hook" for potential future use, add `= default;` or
make it a genuine no-op with a comment explaining why it must exist.

**Lines saved:** 5
**Risk:** None — empty function removal.

---

## Updated Implementation Order (S11–S25)

Recommended order for the new findings, independent of S1–S10:

1. **S11** — Bulk pawn attacks (highest performance impact, isolated change)
2. **S22** — Enable EVAL_PREFETCH in search (trivial, potential perf gain)
3. **S12** — King safety dedup + precompute enemyKingZone (perf + readability)
4. **S21** — Beta-cut stats helper (trivial dedup)
5. **S14** — Move `do_null` before move loop (trivial)
6. **S13** — Remove intermediate variable (trivial)
7. **S23** — Unreachable default branch (trivial)
8. **S25** — Remove empty `reset()` (trivial)
9. **S24** — Extract `relativeRank()` utility (small, cross-cutting)
10. **S16** — `formatDetailedStats()` comprehensive cleanup (large, low-risk, supersedes S4)
11. **S20** — passedPawns fallback (resolved by S1 if done; standalone if not)
12. **S17** — TT aggregate dedup (low priority)
13. **S18** — TT batch aggregation (low priority, cold path)
14. **S19** — History negative values (requires Elo testing — do last, separately)
15. **S15** — Draw check pattern (only if S21 motivates further extraction)

---

## Items Considered and Rejected (S11–S25 pass)

| Item                                                                       | Reason for Rejection                                                                                                                                                                                                                                                              |
|----------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Template `search<NodeType>`**                                            | Templating search by node type would eliminate runtime `nodeType` checks but makes the code significantly harder to read and debug. The branches are well-predicted and the compile-time cost isn't justified for FrankyCPP's current strength tier.                              |
| **Replace `std::ostringstream` in formatDetailedStats with `std::format`** | `std::format` doesn't support all the formatting features used (locale, setprecision in the same call). Would require multiple format calls and wouldn't simplify much.                                                                                                           |
| **Merge `search()` and `qsearch()` into one function**                     | Structurally similar but semantically different enough that merging would hurt readability. The move generation mode (GenAll vs GenNonQuiet), standpat, and depth handling are fundamentally different.                                                                           |
| **Eliminate `tmpScore` member in Evaluator**                               | `tmpScore` is used as a scratch variable in `pawnEval`. Making it local would be cleaner but would add a stack allocation per `pawnEval` call. Since `Score` is only 8 bytes this is negligible, but the current member approach is zero-cost. Low priority, not worth the churn. |

---

*Last updated: 2026-04-11*
