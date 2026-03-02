# VTune Analysis Summary

**Date:** 2026-03-02 16:36:13
**Executable:** FrankyCPP_v1.4.exe
**Configuration:** --bench --threads 8 -l warn -s warn
**Build:** RelWithDebInfo

## Key Performance Metrics

### TT Performance (Primary Focus)

| Metric               | TT::probe | TT::put | TT::prefetch |
|----------------------|-----------|---------|--------------|
| CPU Time (s)         | 42.30     | 13.01   | 0.43         |
| CPI Rate             | 3.37      | 6.65    | 0.33         |
| Microarch Usage (%)  | 5.7%      | 3.4%    | 50.4%        |
| Instructions Retired | 63.7B     | 10.1B   | 6.6B         |

**Atomic Load Overhead:**
- `std::_Atomic_storage::load` (TT key): 26.16s, CPI 4.89
- `std::_Atomic_storage::load` (TT entry): 5.14s, CPI 10.29

### Comparison Baselines

| Metric              | Attacks::sliderLookup | Evaluator::evaluate | Search::search |
|---------------------|-----------------------|---------------------|----------------|
| CPU Time (s)        | 13.74                 | 1.46                | 4.29           |
| CPI Rate            | 0.38                  | 0.31                | 0.44           |
| Microarch Usage (%) | 46.8%                 | 55.8%               | 42.2%          |

### Threading Efficiency

| Metric               | Value             |
|----------------------|-------------------|
| Total Effective Time | ~170s (8 threads) |
| Spin Time            | 0.0s (0.0%)       |
| Wait Time            | 0.0s (0.0%)       |

### Overall Assessment

| Category          | Status       | Notes                                       |
|-------------------|--------------|---------------------------------------------|
| TT Memory Bound   | 🔴 Critical  | CPI 3.37-6.65 in TT ops; 42s in probe alone |
| Thread Contention | 🟢 Good      | Zero spin/wait time observed                |
| Slider Tables     | 🟢 Excellent | CPI 0.38, PEXT working efficiently          |
| Evaluator         | 🟢 Good      | CPI 0.31-0.45, well optimized               |

## Top Functions by CPU Time

1. **TT::probe** - 42.30s (CPI 3.37)
2. **std::_Atomic_storage::load** - 26.16s (CPI 4.89)
3. **Attacks::sliderLookup** - 13.74s (CPI 0.38)
4. **TT::put** - 13.01s (CPI 6.65)
5. **Attacks::attacks** - 9.50s (CPI 0.36)

## Changes Since Baseline

This IS the baseline run (first successful automated VTune script execution).

## Recommendations

1. **Investigate TT bucket implementation** - Current single-slot approach shows extreme CPI (3.37-6.65) indicating memory stalls
2. **Verify prefetch timing** - TT::prefetch shows good CPI (0.33) but may not be effective for probe/put
3. **Consider cache-line alignment** - TT entries may benefit from alignment to reduce cache line splits

---
*Generated from VTune automated analysis script*
