# TC Gauntlet Results

**Date:** 2026-03-30  
**Setup:** Self-play v1.7 vs v1.7_B (identical builds)  
**Settings:** Threads=4, Hash=256 MB, OwnBook=false, Concurrency=4  
**Adjudication:** resign 3/600cp, draw 40/8/10cp  
**Time margin:** 200 ms  
**Opening book:** 8moves_v3.pgn (random order)  

---

## Results

| TC         | Games     | White W     | Black W     | Draws       | Time Loss | Avg Ply | Avg Depth | Max Depth | Wall Time  | Avg/Game |
|------------|-----------|-------------|-------------|-------------|-----------|---------|-----------|-----------|------------|----------|
| **1+0.01** | 500       | 196 (39.2%) | 201 (40.2%) | 103 (20.6%) | **0**     | 106     | 8.2       | 127       | **7 min**  | 0.9s     |
| **2+0.02** | 500       | 179 (35.8%) | 153 (30.6%) | 168 (33.6%) | **0**     | 125     | 10.2      | 31        | **16 min** | 1.9s     |
| **5+0.05** | 500       | 161 (32.2%) | 146 (29.2%) | 193 (38.6%) | **0**     | 136     | 12.3      | 127       | **41 min** | 4.9s     |
| **10+0.1** | 500       | 141 (28.2%) | 124 (24.8%) | 235 (47.0%) | **0**     | 136     | 14.0      | 43        | **81 min** | 9.7s     |
| **60+0.6** | 204       | 52 (25.5%)  | 37 (18.1%)  | 113 (55.4%) | **0**     | 143     | 18.8      | 49        | **3h 30m** | 61.9s    |

> **Note on depth:** Cutechess PGN only records the iterative deepening depth (`depth`), not the
> selective depth (`seldepth`) which includes quiescence search extensions. Seldepth is typically
> 1.5–2× the reported depth. The max=127 values at 1s/5s are instant moves (tablebase hits or
> forced replies), not actual search depths.

---

## Observations

1. **Zero time forfeits across all 2,204 games** — the `timemargin=200` fix is solid.

2. **Draw rate scales correctly with TC:**
   - 1+0.01: 20.6% → 2+0.02: 33.6% → 5+0.05: 38.6% → 10+0.1: 47.0% → 60+0.6: 55.4%
   - More time = deeper search = fewer blunders = more draws. The 55% draw rate at
     LTC is typical of strong engines in self-play.

3. **White advantage is consistent and increases with TC:**
   - At 1s the W/B split is near 50/50 (noise dominates).
   - At 10s White wins 28.2% vs Black 24.8%.
   - At 60s White wins 25.5% vs Black 18.1% — the first-move advantage becomes most
     pronounced as play quality improves and fewer random blunders occur.

4. **Game length plateaus at 5s+:**
   - Avg ply: 106 → 125 → 136 → 136 → 143. Once search depth is sufficient (~12+),
     adjudication catches decisive games at similar points regardless of extra time.

5. **Search depth scales logarithmically:**
   - 8.2 → 10.2 → 12.3 → 14.0 → 18.8. Each doubling of TC adds ~2 ply with SMP
     (4 threads). The 10s→60s jump is +4.8 ply for 6× more time.

6. **Wall time scales linearly with TC** — 7 → 16 → 41 → 81 → 210 min at concurrency=4.

---

## Recommended Development TCs

| Purpose           | TC          | Wall Time       | Draw Rate | Avg Depth | Notes                             |
|-------------------|-------------|-----------------|-----------|-----------|-----------------------------------|
| Smoke test        | **2+0.02**  | 16 min (500g)   | 34%       | 10        | Fast crash/regression detection   |
| Primary A/B test  | **5+0.05**  | 41 min (500g)   | 39%       | 12        | Good balance of speed and quality |
| Confirmation      | **10+0.1**  | 81 min (500g)   | 47%       | 14        | Fishtest STC equivalent           |
| Long confirmation | **60+0.6**  | 3h 30m (200g)   | 55%       | 19        | Fishtest LTC equivalent           |
| 300+0 equivalent  | **180+1.8** | ~12h est (100g) | ~60% est  | ~22 est   | Fishtest VLTC equivalent          |

---

*Generated from PGN files in this directory. See `docs/specs/RESEARCH_Testing_Framework_Settings.md` for methodology.*
