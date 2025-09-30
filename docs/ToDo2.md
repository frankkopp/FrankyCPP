Analysis (current state)
- Fixed now:
    - TT store on null‑move beta cutoffs uses `MOVE_NONE` in 'src/engine/Search.cpp'. Good.
    - NMP is disabled in PV nodes (`!isPv` guard). Good.
    - Mate distance pruning (MDP) present. Good.
    - IID present and only used when no TT move. Good.
    - Aspiration search implemented with widening steps and time‑aware early exit. Good.

- Still likely missing or risky for “mate in 3” at shallow depths:
    - Quiescence lacks selective checks (qchecks). Only captures/promos are mentioned; checks after the capture horizon are critical for mates.
    - LMR safeguards are not visible. Ensure no reductions for checks/captures/promotions, smaller reductions in PV nodes and for early moves.
    - Null‑move verification search is not present. In sharp/mating nets, NMP can prune the mating line; add verification and disable near mate bounds.
    - `goodCapture` SEE threshold should be `>= 0` to not drop equal trades that reveal checks/mates.
    - No explicit check extensions. A small, selective +1 ply on checking moves with few evasions helps.
    - Move ordering: ensure TT move, then good checks, then good captures (SEE), then killers/history; checks must be high priority.
    - Futility/razoring guards: already skip in check, but also avoid pruning where the side to move can give check.
    - Aspiration widening is asymmetric on fail‑low vs fail‑high; make it symmetric and conservative under low time.
    - `matethreat` is set but not used to temper pruning/reductions.

Step‑by‑step plan with prompts

Step 1: Add selective checks to quiescence (qchecks)
Rationale: Without qchecks, shallow mates are often beyond the capture horizon. Searching safe checking moves after captures uncovers mating nets.

Changes (in 'src/engine/Search.cpp'): extend `qsearch` to try non‑losing captures/promos first, then selective checking moves that are not captures/promos.

Explanation: The code adds a second phase in qsearch to generate quiet checking moves (by filtering all pseudo‑legal moves for those that give check and are not capture/promo). Adapt API calls (e.g., checking if a move gives check) to your Position/MoveGenerator.

```cpp
// C++
Value Search::qsearch(Position& p, const Depth ply, Value alpha, Value beta, const Node_Type isPv) {
  nodesVisited++;

  const bool inCheck = p.hasCheck();
  Value best = VALUE_MIN;

  // If in check: search all legal evasions (full q-node)
  if (inCheck) {
    MoveGenerator localMg;
    Move m;
    int legal = 0;
    while ((m = localMg.getNextPseudoLegalMove(p, GenAll, /*hasCheck*/true)) != MOVE_NONE) {
      p.doMove(m);
      if (p.isIllegal()) { p.undoMove(); continue; }
      legal++;
      Value score = -qsearch(p, ply + 1, -beta, -alpha, isPv);
      p.undoMove();
      if (score >= beta) return beta;
      if (score > alpha) alpha = score;
    }
    return legal ? alpha : evaluate(p); // stalemate or checkmate handled in main search
  }

  // Stand pat
  Value stand = evaluate(p);
  if (stand >= beta) return beta;
  if (stand > alpha) alpha = stand;

  // Phase 1: captures/promotions (keep equal or better by SEE)
  {
    MoveGenerator localMg;
    Move m;
    while ((m = localMg.getNextPseudoLegalMove(p, GenTactical, /*hasCheck*/false)) != MOVE_NONE) {
      if (!goodCapture(p, m)) continue; // make this SEE >= 0
      p.doMove(m);
      if (p.isIllegal()) { p.undoMove(); continue; }
      Value score = -qsearch(p, ply + 1, -beta, -alpha, isPv);
      p.undoMove();
      if (score >= beta) return beta;
      if (score > alpha) alpha = score;
    }
  }

  // Phase 2: selective quiet checks (qchecks)
  {
    MoveGenerator localMg;
    Move m;
    while ((m = localMg.getNextPseudoLegalMove(p, GenAll, /*hasCheck*/false)) != MOVE_NONE) {
      // Adapt these helpers to your API:
      const bool isTactical = m.isPromotion() || p.isCapture(m);
      if (isTactical) continue;

      // Determine if move gives check (cheap test or make/unmake fallback)
      bool givesCheck = false;
      // If you have a fast "gives check" detector, use it here instead of make/unmake:
      p.doMove(m);
      if (!p.isIllegal()) givesCheck = p.hasCheck();
      p.undoMove();

      if (!givesCheck) continue;

      p.doMove(m);
      if (p.isIllegal()) { p.undoMove(); continue; }
      Value score = -qsearch(p, ply + 1, -beta, -alpha, isPv);
      p.undoMove();
      if (score >= beta) return beta;
      if (score > alpha) alpha = score;
    }
  }

  return alpha;
}
```

Prompt for step 1:
- Please modify 'src/engine/Search.cpp' to add selective checks in `qsearch` after the capture phase, as in the snippet. Use your existing MoveGenerator/Position API for: generating tactical moves, detecting captures/promotions, and checking “move gives check” (replace the make/unmake probe if you have a faster helper). I will build and test locally in CLion and report back.

Step 2: Make LMR tactics‑friendly and safer
Rationale: Over‑reducing quiet forcing moves (especially checks) hides mates until deeper iterations.

Changes (in the normal move loop inside `search`):
- Do not reduce checks, captures, or promotions.
- Reduce less in PV nodes and for early moves.
- Optionally scale down reductions when the position is improving.

Explanation: Compute a base reduction and clamp it to 0 for forcing moves and PV/early moves.

```cpp
// C++
// Inside the move loop of Search::search(...)
int moveIndex = /* 1-based index of legal moves searched so far */;
const bool isCapture = p.isCapture(move);
const bool isPromo   = move.isPromotion();

// Determine if move gives check (prefer a fast detector if available)
bool givesCheck = false;
p.doMove(move);
if (!p.isIllegal()) givesCheck = p.hasCheck();
p.undoMove();

// Base reduction
int r = 0;
if (!isPv && depth >= 3 && moveIndex > 3 && !isCapture && !isPromo && !givesCheck) {
  // Example reduction schedule; tune empirically
  r = 1 + (depth >= 5) + (moveIndex >= 8);
  // Improving heuristic: reduce less if static eval improved vs parent
  if (/* improving */ false) r = std::max(0, r - 1);
}

// Final child depth with optional extension (see Step 5)
int ext = 0;
Depth childDepth = depth - 1 + ext - r;

// PVS / zero-window handling as before, using childDepth
```

Prompt for step 2:
- Please update the LMR logic in the move loop of `Search::search` in 'src/engine/Search.cpp': skip reductions for checks/captures/promotions, reduce less in PV nodes and for early moves, and apply a simple reduction schedule like in the snippet. Keep existing PVS logic. I will build and test locally and report back.

Step 4: Improve move ordering priority for checks
Rationale: Checks searched early uncover mates at shallow depths.

Changes:
- Ensure TT move is tried first.
- Score moves so that checking moves come right after TT move and winning captures/promotions.

Explanation: Use a simple ordering score; integrate with your existing History/Killers.

```cpp
// C++
// Example scoring function used by your move picker:
inline int scoreMove(const Position& p, Move m, Move ttMove) {
  if (m == ttMove) return 1'000'000;

  int s = 0;
  if (m.isPromotion()) s += 200'000;
  if (p.isCapture(m))  s += 150'000;

  // Prefer giving check
  bool givesCheck = false;
  p.doMove(m);
  if (!p.isIllegal()) givesCheck = p.hasCheck();
  p.undoMove();
  if (givesCheck) s += 120'000;

  // SEE for captures (winning or equal)
  if (p.isCapture(m)) s += std::max(0, See::see(p, m)) * 100;

  // Killers / History (fill from your History)
  s += history.score(p, m);

  return s;
}
```

Prompt for step 4:
- Please adjust move ordering in 'src/engine/Search.cpp' so the move picker scores checking moves just after TT/good captures/promotions, as in the sample scoring. Keep your History/Killers in the score. I will build and test and report back.

Step 5: Add selective check extension (+1 ply)
Rationale: Extending checking moves with few legal replies helps confirm short mates at D6.

Changes (in move loop of `search`):
- If a move gives check and the opponent has ≤ 2 legal replies, extend by +1 ply.

```cpp
// C++
// After detecting 'givesCheck'
int ext = 0;
if (givesCheck && depth >= 2) {
  p.doMove(move);
  if (!p.isIllegal()) {
    auto* replies = mg[ply + 1].generateLegalMoves(p, GenAll);
    const int replyCount = static_cast<int>(replies->size());
    if (replyCount <= 2) ext = 1;
  }
  p.undoMove();
}

Depth childDepth = depth - 1 + ext - r;
```

Prompt for step 5:
- Please add a small check extension in the `search` move loop in 'src/engine/Search.cpp': if a move gives check and the opponent has ≤ 2 legal replies, extend by +1 ply. Keep it selective as in the snippet. I will build and test and report back.

Step 6: Make `goodCapture` keep equal trades and never drop checking captures
Rationale: Equal exchanges can be the entry to a forced checking sequence; dropping them hides mates.

Changes (in 'src/engine/Search.cpp'):
- Ensure SEE threshold is `>= 0`.
- Always keep captures that give check.

```cpp
// C++
bool Search::goodCapture(Position& p, const Move move) {
  // If capture gives check, always keep it
  bool givesCheck = false;
  p.doMove(move);
  if (!p.isIllegal()) givesCheck = p.hasCheck();
  p.undoMove();
  if (givesCheck) return true;

  // Otherwise use SEE >= 0
  return See::see(p, move) >= 0;
}
```

Prompt for step 6:
- Please adjust `goodCapture` in 'src/engine/Search.cpp' to use SEE `>= 0` and to always keep captures that give check, as in the snippet. I will build and test and report back.

Step 7: Futility/razoring safeguards around checks
Rationale: Futility/razoring can drop quiet checking moves at shallow depth.

Changes:
- Keep current “skip if in check”.
- Additionally skip RFP/futility at depth ≤ 3 when a checking move is available to the side to move (lightweight probe), and never apply futility to moves that give check.

```cpp
// C++
// Before RFP/razoring decisions (lightweight probe)
bool hasAvailableCheck = false;
{
  MoveGenerator probe;
  Move m;
  while ((m = probe.getNextPseudoLegalMove(p, GenAll, /*hasCheck*/false)) != MOVE_NONE) {
    bool gives = false;
    p.doMove(m);
    if (!p.isIllegal()) gives = p.hasCheck();
    p.undoMove();
    if (gives) { hasAvailableCheck = true; break; }
  }
}

// In your RFP condition add: && !hasAvailableCheck
// And in move loop: never apply futility to a move if givesCheck == true
```

Prompt for step 7:
- Please guard RFP/futility/razoring in 'src/engine/Search.cpp' so they are skipped when a checking move is available for the side to move, and never apply futility to checking moves. I will build and test and report back.

Step 8: Symmetric aspiration widening with time guard
Rationale: Symmetric widening reduces re‑search spikes and keeps PV stable.

Changes (in `aspirationSearch` in 'src/engine/Search.cpp'):
- Widen both sides stepwise on both fail‑low and fail‑high, unless `isTimeAlmostUp()`.

```cpp
// C++
Value Search::aspirationSearch(Position& p, const Depth depth, const Value bestValue) {
  constexpr std::array steps = {Value{50}, Value{100}, Value{200}, VALUE_MAX};
  Value value = VALUE_NONE;

  Value alpha = std::max(bestValue - steps[0], VALUE_MIN);
  Value beta  = std::min(bestValue + steps[0], VALUE_MAX);

  for (size_t i = 1; i < steps.size(); ++i) {
    value = rootSearch(p, depth, alpha, beta);
    if (stopConditions()) return (value > alpha && value < beta) ? value : VALUE_NONE;
    if (value > alpha && value < beta) return value;

    if (isTimeAlmostUp()) return value;

    // Symmetric widening
    alpha = std::max(bestValue - steps[i], VALUE_MIN);
    beta  = std::min(bestValue + steps[i], VALUE_MAX);
    statistics.aspirationResearches++;
  }
  return value;
}
```

Prompt for step 8:
- Please make aspiration widening symmetric in 'src/engine/Search.cpp' as in the snippet, with a time guard to avoid last‑second expansions. I will build and test and report back.

Step 9: Use `matethreat` to temper reductions and disable NMP one ply
Rationale: If null‑move indicates a mate threat, be conservative: avoid LMR and NMP in the immediate child to preserve tactics.

Changes:
- When `matethreat` is set at a node, pass a flag to children to reduce or skip reductions/NMP once, or simply clamp `r = 0` and set `doNull = No_Null_Move` for the next ply.

Prompt for step 9:
- Please use the existing `matethreat` flag in 'src/engine/Search.cpp' to disable NMP and LMR for one ply after it’s detected, so tactical threats are not pruned away. I will build and test and report back.

Step 10: Minor reliability and diagnostics
Rationale: Stability and tuning.

Changes:
- Timer: ensure it sleeps/yields in tight loops to avoid pegging a core.
- TT: store QS entries with `DEPTH_NONE` and mate‑distance‑normalized scores; confirm `valueToTt/valueFromTt` use ply adjustment.
- UCI updates: send periodic info every ~200–300 ms or based on nodes delta for better feedback.
- Add counters for NMP verifications and LMR re‑searches to help tune.

Prompt for step 10:
- Please harden the timer (sleep/yield), verify TT mate distance normalization for `valueToTt/valueFromTt`, and add basic counters/logs for NMP verification and LMR re‑search rates in 'src/engine/Search.cpp'. I will build and test and report back.

Notes
- Where I used `p.isCapture(m)`, `m.isPromotion()`, or “gives check” detection, adapt to your actual API (or keep the conservative make/unmake probe).
- Keep changes small per step; after each step I expect build and test results from your CLion environment.
