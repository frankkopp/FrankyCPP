# FrankyCPP Search Features Review

**Created:** 2026-02-19  
**Updated:** 2026-02-20  
**Purpose:** Systematic review of all search features for correctness and comparison to other engines (Stockfish, etc.)

---

## Background

Starting from a baseline of +30 ELO vs v1.1 (from TB + extensions in v1.2), several improvements were made:

| Change                         | ELO vs v1.1 | Test Suite      | Notes                          |
|--------------------------------|-------------|-----------------|--------------------------------|
| Baseline (TB + extensions)     | +30         | -               | Starting point (v1.2)          |
| After LMR log formula          | +64         | +2.2% (+63 pos) | 6 suites improved, 0 regressed |
| After LMR tuning (1.5 divisor) | +82         | +1.5% (+43 pos) | 5 improved, 1 regressed        |
| After isPvNode fix             | +109        | +0.3% (+10 pos) | 3 improved, 3 regressed        |

**Final result: +109 ELO** vs v1.1 baseline

### Bugs Fixed

1. **Razoring**: Used `PV` instead of `NonPV`, missing `!isPvNode` guard
2. **PVS first move**: Hardcoded `PV` instead of inheriting `isPvNode`
3. **LMR/PVS re-searches**: Hardcoded `PV` instead of inheriting `isPvNode`
4. **History penalty**: Penalized alpha-raising moves (best move got penalty)
5. **History on captures**: Applied history to captures (should be quiet moves only)

**Result:** PV node ratio went from 20% → 0.02%

This document tracks further review of all search features for additional improvements.

---

## Feature List

| #  | Feature                                | Description                                                                                                                   | To Check                                                         |
|----|----------------------------------------|-------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------|
| 1  | **Mate Distance Pruning (MDP)**        | Tightens alpha/beta bounds based on ply distance to avoid searching for mates longer than already found                       | Compare formula to Stockfish                                     |
| 2  | **TT Lookup & Cutoff**                 | Uses transposition table to avoid re-searching positions; `!isPvNode` guard prevents cutoffs on PV nodes                      | Verify depth replacement scheme, aging                           |
| 3  | **Tablebase Probing**                  | Probes Syzygy tablebases for endgame positions; only cuts off on non-PV nodes                                                 | Check probe depth threshold, DTZ vs WDL usage                    |
| 4  | **Razoring**                           | Jumps to qsearch when static eval is far below alpha at depth 1                                                               | Compare margin to Stockfish; check if depth 1 only is optimal    |
| 5  | **Reverse Futility Pruning (RFP)**     | Returns early when static eval is far above beta; guards: `!isPvNode`, `!hasCheck`, `doNull`, `depth <= 3`                    | Compare margins per depth to Stockfish                           |
| 6  | **Null Move Pruning (NMP)**            | Skips a move to prove position is good enough for beta cutoff; has zugzwang guard, mate threat detection, verification search | Compare reduction formula (R), verify depth threshold            |
| 7  | **Internal Iterative Deepening (IID)** | Finds a good move to search first when no TT move available; PV-only by design                                                | Consider IIR (reductions) on all nodes like Stockfish            |
| 8  | **Check Extension**                    | Extends search by 1 ply when move gives check; limited to first N moves                                                       | Compare limit to other engines; check if SEE filter needed       |
| 9  | **Threat Extension**                   | Extends when mate threat detected from NMP; disabled by default                                                               | Evaluate if should be enabled or removed                         |
| 10 | **Singular Extension**                 | Extends TT move if it's significantly better than alternatives                                                                | Compare margin, depth threshold, reduction to Stockfish          |
| 11 | **Futility Pruning (FP)**              | Prunes moves unlikely to raise alpha based on static eval + margin; `depth < 7`                                               | Compare margins per depth; check if depth limit is optimal       |
| 12 | **Late Move Pruning (LMP)**            | Prunes late moves entirely based on move count                                                                                | Compare move count thresholds to Stockfish                       |
| 13 | **Late Move Reduction (LMR)**          | Reduces search depth for late quiet moves; uses log formula                                                                   | Compare formula; add "improving" flag; history-based adjustments |
| 14 | **PVS (Principal Variation Search)**   | Searches first move full-window, others null-window with re-search                                                            | Verify re-search conditions                                      |
| 15 | **History Heuristic**                  | Tracks move success for ordering quiet moves; bonus on cutoff, penalty on fail                                                | Compare bonus/penalty formula; consider gravity/aging            |
| 16 | **Killer Moves**                       | Stores quiet moves that caused beta cutoff for sibling nodes                                                                  | Verify 2 killers is optimal; check slot replacement              |
| 17 | **Counter Moves**                      | Stores refutation moves keyed by opponent's last move                                                                         | Consider adding counter-move history                             |
| 18 | **Quiescence Search**                  | Searches captures/checks until position is quiet                                                                              | Check stand-pat, delta pruning, SEE threshold                    |
| 19 | **Draw Detection**                     | Checks repetition and 50-move rule                                                                                            | Verify repetition count (2 vs 3); contempt handling              |
| 20 | **TT Storage**                         | Stores search results with mate score adjustment                                                                              | Verify replacement strategy, bound types                         |
| 21 | **Move Ordering**                      | Orders moves: TT → Captures (MVV-LVA/SEE) → Killers → Counter → History                                                       | Compare to Stockfish order; check capture history                |
| 22 | **Aspiration Windows**                 | Narrows search window around previous score                                                                                   | Check window size, widening strategy                             |
| 23 | **Time Management**                    | Allocates time per move based on game phase                                                                                   | Review complexity factor, instability handling                   |
| 24 | **Static Eval**                        | Position evaluation function                                                                                                  | Separate review needed                                           |

---

## Priority Order for Review

1. **LMR** - Highest impact on tree size (from analysis plan)
2. **Move Ordering** - Directly affects cutoff efficiency
3. **NMP** - Major pruning technique
4. **History Heuristic** - Affects move ordering quality
5. **Futility Pruning / RFP** - Common pruning techniques
6. **Singular Extension** - Can significantly affect tactical strength

---

## Notes

- **+109 ELO total** vs v1.1 achieved through LMR formula change and isPvNode fixes
- PV node ratio fix (20% → 0.02%) enabled pruning to work correctly
- Test suite improvement (+10 positions) is modest but ELO gain is significant
- The combination of correct PV flagging + more aggressive LMR provides the best results

---

## Comparison Resources

- [Stockfish source](https://github.com/official-stockfish/Stockfish)
- [Chess Programming Wiki](https://www.chessprogramming.org/)
- [Ethereal source](https://github.com/AndyGrant/Ethereal)
