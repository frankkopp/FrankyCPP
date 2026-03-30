# Research: Chess Engine Testing Framework Settings

**Date:** 2026-03-30  
**Context:** Finding optimal time controls for FrankyCPP development testing via cutechess-cli self-play gauntlets.

---

## 1. Fishtest (Stockfish Testing Framework)

Fishtest is the distributed testing framework used by the Stockfish project. It runs thousands of games across volunteer machines to validate patches.

### Standard Time Controls
| Label                | Time Control | Increment  | Hash   | Threads | Games/Test      |
|----------------------|--------------|------------|--------|---------|-----------------|
| **STC** (Short)      | 10+0.1       | 1% of base | 16 MB  | 1       | 40,000–60,000   |
| **LTC** (Long)       | 60+0.6       | 1% of base | 64 MB  | 1       | 40,000+         |
| **VLTC** (Very Long) | 180+1.8      | 1% of base | 256 MB | 1       | Rare            |
| **SMP STC**          | 10+0.1       | 1% of base | 64 MB  | 8       | Regression only |

### Key Settings
- **Increment = 1% of base time** — consistent ratio across all TCs
- **Threads = 1** for SPRT regression testing (minimizes variance over 40k+ games; see Section 4 for why FrankyCPP uses multi-threaded)
- **SMP tests** only for threading-specific patches
- **Hash = 16–64 MB** — small for cross-machine consistency on volunteer hardware
- **Adjudication:** resign 3 moves / 600cp, draw after move 40 / 8 moves / 10cp
- **Opening book:** `noob_3moves.epd` (varied positions, 3-move openings)
- **SPRT** (Sequential Probability Ratio Test): Tests stop when statistical significance is reached
  - Typical bounds: `elo0=0, elo1=2` for STC; `elo0=0, elo1=1.5` for LTC
  - Not fixed game count — games continue until SPRT decides
- **Time margin:** cutechess `-timemargin 200` (compensates for OS scheduling jitter)

### Fishtest Workflow
1. STC test first (fast, cheap — 10+0.1)
2. If STC passes → LTC confirmation (60+0.6)
3. Both must pass SPRT for patch acceptance
4. Non-functional patches (e.g., cleanup) use `elo0=-1.5, elo1=0.5` (simplification test)

---

## 2. OpenBench (Multi-Engine Testing Framework)

OpenBench is used by many engines (Ethereal, Berserk, Weiss, Koivisto, etc.) and supports distributed testing similar to Fishtest.

### Standard Time Controls
| Label   | Time Control | Hash   | Threads  |
|---------|--------------|--------|----------|
| **STC** | 8+0.08       | 16 MB  | 1        |
| **LTC** | 40+0.4       | 128 MB | 1        |
| **SMP** | 20+0.2       | 128 MB | Variable |

### Key Settings
- **Increment = 1% of base** (same convention as Fishtest)
- **SPRT bounds:** Typically `elo0=0, elo1=5` (less conservative than Fishtest)
- **Adjudication:** resign 5 moves / 500cp, draw similar to Fishtest
- **Time margin:** `-timemargin 300` (often higher due to diverse hardware)

---

## 3. Cutechess-cli Settings

### Critical Parameters for Fair Testing
| Parameter                                  | Recommended                   | Notes                                      |
|--------------------------------------------|-------------------------------|--------------------------------------------|
| `-timemargin`                              | 200–350 ms                    | Prevents false time losses from OS jitter  |
| `-recover`                                 | Yes                           | Restart engine on crash instead of forfeit |
| `-concurrency`                             | 2–4                           | Number of simultaneous games               |
| `-repeat`                                  | Yes (implicit with `-rounds`) | Play each opening from both sides          |
| `-draw movenumber=40 movecount=8 score=10` | Standard                      | Adjudicate draws                           |
| `-resign movecount=3 score=600`            | Standard                      | Adjudicate resignations                    |
| `-pgnout`                                  | path.pgn                      | Save games for analysis                    |
| `-openings file=X format=pgn order=random` | Randomized                    | Avoid systematic bias                      |

### Time Margin Guidance
- **200 ms**: Sufficient for fast hardware, low concurrency, single-threaded engines
- **300 ms**: Safe default for development machines under moderate load
- **350+ ms**: Use when running many concurrent games or with background load
- **Per-engine overhead measurement**: FrankyCPP logs `post-stop overhead` which directly measures the time from `stopSearchFlag=true` to `bestmove` output — use this to calibrate

### Self-Play Testing
For A/B testing of patches within the same engine:
- Use **two separate copies** of the engine binary (e.g., `v1.7/` and `v1.7_B/`)
- This avoids file locking issues (opening book cache, log files)
- Both copies should be from the **same build** for baseline, or A vs B for patch testing
- Use `-each` option to apply common settings to both engines
- Default to **Threads=4, Hash=128 MB** for realistic strength measurement

---

## 4. Recommended FrankyCPP Development Testing Protocol

### Why Fishtest Uses Threads=1 (And Why We Don't Have To)

Fishtest/OpenBench use **Threads=1, small Hash** because:
- They run on **thousands of diverse volunteer machines** — small hash ensures consistency
- They test **individual patches** for tiny Elo gains (±2 Elo) over 40,000+ games
- Threads=1 **reduces variance** from non-deterministic thread scheduling
- Their SPRT framework needs the cleanest possible signal

**FrankyCPP development testing differs:**
- Single dev machine with known hardware — no cross-machine consistency needed
- Fewer games (500–2000), detecting larger Elo differences (±10–20)
- SMP (Lazy SMP) is a **major strength factor** — testing without it misses real regressions
- Goal is often overall strength measurement or TC optimization, not isolating a single patch

**Rule of thumb:**
- **Threads=1:** Only when A/B testing a specific single-threaded algorithm change with tight Elo bounds
- **Threads=4+:** For all other testing (TC calibration, overall strength, SMP-aware features)

### Thread/Hash Guidelines for FrankyCPP

| Test Purpose                      | Threads | Hash       | Rationale                    |
|-----------------------------------|---------|------------|------------------------------|
| TC calibration (self-play)        | 4       | 128 MB     | Match real play conditions   |
| Overall strength vs baseline      | 4       | 128 MB     | Representative of actual use |
| Isolating search algorithm change | 1       | 64 MB      | Minimize SMP noise           |
| SMP-specific patch (threading)    | 4–8     | 128–256 MB | Must test what changed       |
| Eval tuning                       | 1       | 64 MB      | Eval is single-threaded      |

### Concurrency vs Threads

With cutechess `-concurrency`, total CPU threads = `concurrency × threads_per_engine × 2 engines`.
Leave headroom for OS + cutechess overhead:
- **16-core machine:** concurrency=4, Threads=4 → 32 engine threads (reasonable with HT)
- **8-core machine:** concurrency=2, Threads=4 → 16 engine threads
- Reduce concurrency before reducing threads — thread count affects engine strength

### Phase 1: Quick Smoke Test (Bullet)
- **TC:** 2+0.02 (or 1+0.01 for ultra-fast)
- **Games:** 500–1000 rounds (1000–2000 games)
- **Purpose:** Catch crashes, major regressions, time management issues
- **Threads:** 4, Hash: 64 MB
- **Expected duration:** ~30–60 min at concurrency 4

### Phase 2: Standard Development Test (Blitz/STC)
- **TC:** 5+0.05 or 10+0.1
- **Games:** 500+ rounds
- **Purpose:** Primary Elo measurement
- **Threads:** 4, Hash: 128 MB
- **Expected duration:** ~2–4 hours at concurrency 4

### Phase 3: Confirmation (LTC)
- **TC:** 60+0.6
- **Games:** 200+ rounds
- **Purpose:** Confirm gains are real, not just tactical noise
- **Threads:** 4, Hash: 128 MB
- **Expected duration:** ~8–12 hours at concurrency 4

### Phase 4: SMP Validation (if applicable)
- **TC:** 10+0.1
- **Games:** 200+ rounds
- **Threads:** 8, Hash: 256 MB
- **Purpose:** Verify SMP scaling, no threading regressions

### General Rules
- **Threads=4** as default for development testing (matches real play, includes SMP)
- **Threads=1** only for isolating single-threaded algorithm changes
- **OwnBook=false** — use cutechess opening book for reproducibility
- **Adjudication ON** — saves time without affecting Elo accuracy
- **Self-play** with two engine copies for A/B testing
- **Time margin 200ms** as baseline, increase if time losses observed
- **Hash=128 MB** as standard (enough for STC/LTC depths, not wasteful)

---

## 5. Increment Convention

The **1% increment rule** (increment = 1% of base time) is nearly universal:

| Base Time | Increment |
|-----------|-----------|
| 1s        | 0.01s     |
| 2s        | 0.02s     |
| 5s        | 0.05s     |
| 10s       | 0.1s      |
| 30s       | 0.3s      |
| 60s       | 0.6s      |
| 180s      | 1.8s      |

**Why 1%?** It's enough to prevent trivial time losses in won endgames while being small enough that time pressure remains meaningful for testing time management.

---

## 6. Statistical Significance

### SPRT vs Fixed Game Count
- **SPRT** (Sequential Probability Ratio Test): Stop when statistically significant — faster for clear wins/losses
- **Fixed count:** Always play N games — simpler, but wastes games on obvious results

### Minimum Games for Reliable Elo
| Elo Difference | Min Games (95% confidence) |
|----------------|----------------------------|
| ±50 Elo        | ~100 games                 |
| ±20 Elo        | ~500 games                 |
| ±10 Elo        | ~2,000 games               |
| ±5 Elo         | ~10,000 games              |
| ±2 Elo         | ~40,000+ games             |

For FrankyCPP development, **500–1000 rounds (1000–2000 games)** at STC is a practical sweet spot for detecting ±10–20 Elo changes.

---

*References: Fishtest documentation, OpenBench source, cutechess-cli manual, chess programming wiki*
