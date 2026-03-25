# FrankyCPP Tuning Dataset — Extraction Commands

The extracted dataset files are **not committed to git** (too large).  
Use these commands to regenerate them from the source PGN/archives.

---

## Prerequisites

- Build the Extractor target:
  ```powershell
  cmake --build cmake-build-win-release --target FrankyCPP_v1.7_Extractor
  ```
- Decompress `selfplay_v1.7_50k.7z` → `selfplay_v1.7_50k.pgn` (if PGN not present)
- Decompress `quiet-labeled.epd.7z` → `quiet-labeled.epd` (if EPD not present)

---

## 1. Self-Play Dataset (Primary — 4.57M positions)

50,000 v1.7 self-play games, extracted with score + qsearch filters.

```powershell
& "D:\_DEV\FrankyCPP\cmake-build-win-release\src\FrankyCPP_v1.7_Extractor.exe" `
  --input "D:\_DEV\FrankyCPP\test\testsets\tuning\selfplay_v1.7_50k.pgn" `
  --output "D:\_DEV\FrankyCPP\test\testsets\tuning\selfplay_v1.7_50k_score.txt" `
  --score-filter --qsearch-filter --verbose
```

| Detail | Value |
|---|---|
| **Output file** | `selfplay_v1.7_50k_score.txt` |
| **Positions extracted** | 4,568,763 |
| **File size** | ~277 MB |
| **Runtime** | ~88s (570 games/s with qsearch filter) |

### Source PGN Details

See `50k-matches-info.txt` for full match statistics.

- 50,142 games total (50,000 scheduled rounds)
- FrankyCPP v1.7 self-play, `st=0.5`, concurrency 12
- Opening book: `8moves_v3.pgn` (random order)
- 49,695 games processed after filtering (unknown results, time forfeits)

### Filters Applied

| Filter | Setting | Removed |
|---|---|---|
| Min half-moves | 16 (8 full moves) | 9.65% |
| Skip captures/promotions | yes | 13.60% |
| Skip positions in check | yes | 8.51% |
| Min pieces | 6 | 6.72% |
| Qsearch stability | 150 cp threshold | 2.26% |
| Score contradiction | 200 cp threshold | 0.11% |
| **Total filtered** | | **40.86%** |

### Extraction Summary (2026-03-25)

```
Games:  50,142 total → 49,695 processed (99.11%)
  White wins: 16,103 (32.40%)
  Black wins: 14,129 (28.43%)
  Draws:      19,463 (39.16%)

Positions: 7,724,774 seen → 4,568,763 extracted (59.14%)
```

---

## 2. Zurichess Dataset (Supplemental — 1.43M positions)

No extraction needed — already in EPD format (`quiet-labeled.epd`).  
Decompress from `quiet-labeled.epd.7z` if not present.

| Detail | Value |
|---|---|
| **Source** | https://bitbucket.org/zurichess/tuner/downloads/quiet-labeled.v7.epd.gz |
| **Positions** | 1,428,000 |
| **Format** | EPD with `c9` tag (auto-detected by `TuningDataset` loader) |

See `Zurichess dataset quiet-labeled.v7.txt` for details.

---

## 3. Dev Datasets (Small Subsets)

Smaller extractions from earlier match PGNs, used during Phase 4 extractor development.  
Kept for reference and fast-iteration testing.

| File | Positions | Source |
|---|---|---|
| `v1.6_vs_v1.5_score.txt` | ~49K | v1.6 vs v1.5, 500 games, score filter |
| `v1.6_vs_v1.5.txt` | ~49K | v1.6 vs v1.5, 500 games, no score filter |
| `v1.6_vs_v1.5_qs.txt` | ~47K | v1.6 vs v1.5, 500 games, qsearch filter |
| `v1.6_vs_SF18.txt` | ~44K | v1.6 vs SF18, 500 games, no score filter |
| `v1.6_vs_SF18_qs.txt` | ~41K | v1.6 vs SF18, 500 games, qsearch filter |

---

## Combined Dataset for Tuning (~6.0M positions)

| Dataset | Positions | Source |
|---|---|---|
| `selfplay_v1.7_50k_score.txt` | 4,568,763 | Self-play (primary) |
| `quiet-labeled.epd` | 1,428,000 | Zurichess/CCRL (supplemental) |
| **Total** | **~5,996,763** | Mixed |

---

*Last updated: 2026-03-25*
