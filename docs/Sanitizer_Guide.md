# Sanitizer Guide for FrankyCPP

This guide explains how to use memory and thread sanitizers to debug crashes and race conditions.

---

## Quick Start

### For Threading Issues (Data Races) - Use TSAN in WSL

```bash
# In WSL terminal:
cd /mnt/d/_DEV/FrankyCPP

# Configure with TSAN
cmake --preset wsl-debug-tsan

# Build (test discovery is skipped automatically for TSAN builds)
cmake --build cmake-build-wsl-debug-tsan

# Run tests with ASLR disabled (required on WSL2)
# Option 1: Use setarch to disable ASLR for this process only
setarch $(uname -m) -R ./cmake-build-wsl-debug-tsan/test/FrankyCPP_v1.6_Test --gtest_filter=-*SpeedTests.*:-*TimingTests.*

# Option 2: Disable ASLR system-wide (revert with value 2)
sudo sysctl -w kernel.randomize_va_space=0
./cmake-build-wsl-debug-tsan/test/FrankyCPP_v1.6_Test --gtest_filter=-*SpeedTests.*:-*TimingTests.*
sudo sysctl -w kernel.randomize_va_space=2  # Re-enable after testing

# Run specific tests (e.g., SMP tests only)
setarch $(uname -m) -R ./cmake-build-wsl-debug-tsan/test/FrankyCPP_v1.6_Test --gtest_filter=*SMP*
```

### For Memory Corruption (Use-After-Free, Buffer Overflow) - Use ASAN

**Windows (MSVC):**
```powershell
# In PowerShell:
cmake --preset win-debug-asan
cmake --build cmake-build-win-debug-asan

# Run tests - ASAN will print detailed reports on errors
.\cmake-build-win-debug-asan\test\FrankyCPP_v1.6_Test.exe

.\cmake-build-win-relwithdebinfo-asan\test\FrankyCPP_v1.6_Test.exe --gtest_filter=-*SpeedTests.*:-*TimingTests.*
```

**WSL (GCC/Clang):**
```bash
cmake --preset wsl-debug-asan
cmake --build cmake-build-wsl-debug-asan
./cmake-build-wsl-debug-asan/test/FrankyCPP_v1.6_Test
```

---

## Available Presets

| Preset                    | Platform     | Sanitizer        | Use Case                                    |
|---------------------------|--------------|------------------|---------------------------------------------|
| `win-debug-asan`          | Windows/MSVC | AddressSanitizer | Memory bugs on Windows                      |
| `win-relwithdebinfo-asan` | Windows/MSVC | AddressSanitizer | Memory bugs with optimization               |
| `wsl-debug-tsan`          | Linux/GCC    | ThreadSanitizer  | **Data races (recommended for your issue)** |
| `wsl-relwithdebinfo-tsan` | Linux/GCC    | ThreadSanitizer  | Data races with optimization                |
| `wsl-debug-asan`          | Linux/GCC    | ASan + UBSan     | Memory bugs on Linux                        |
| `wsl-clang-debug-tsan`    | Linux/Clang  | ThreadSanitizer  | Data races (alternative)                    |

---

## Understanding Your Crash

Your crash symptoms:
- Exit code `-1073741819` (`0xC0000005`) = Windows Access Violation
- Only with `THREADS > 1` (Lazy SMP)
- Random timing (early or deep in search)
- Different tests fail each run

**Most likely causes:**
1. **Data race** - Two threads accessing shared data without synchronization
2. **Use-after-free** - One thread frees memory another is using
3. **Double-free** - Multiple threads freeing the same memory

**TSAN is the best tool here** because it detects data races before they cause crashes.

---

## Reading TSAN Output

A typical TSAN report looks like:

```
==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 8 at 0x7f1234567890 by thread T2:
    #0 Search::updateBestMove(Move) Search.cpp:456
    #1 Search::searchRoot() Search.cpp:123
    #2 Search::threadMain() Search.cpp:89

  Previous read of size 8 at 0x7f1234567890 by thread T1:
    #0 Search::getBestMove() Search.cpp:234
    #1 UciHandler::processGo() UciHandler.cpp:567

  Location is global 'Search::bestMove' of size 8 at 0x7f1234567890
==================
```

**How to read it:**
1. **Type**: "data race" - two threads accessing same memory unsafely
2. **Thread T2 writes**: Shows the write operation and call stack
3. **Thread T1 reads**: Shows the conflicting read operation
4. **Location**: What variable/memory is involved

**Fix approach:** Add synchronization (mutex, atomic, etc.) around the shared variable.

---

## Reading ASAN Output

A typical ASAN report:

```
==12345==ERROR: AddressSanitizer: heap-use-after-free
READ of size 8 at 0x60200000ef80 thread T0
    #0 Search::evaluate() Search.cpp:789
    #1 Search::alphaBeta() Search.cpp:456
    
0x60200000ef80 is located 0 bytes inside of 64-byte region [0x60200000ef80,0x60200000efc0)
freed by thread T1 here:
    #0 operator delete(void*)
    #1 Position::~Position() Position.cpp:123
    
previously allocated by thread T0 here:
    #0 operator new(unsigned long)
    #1 Position::Position() Position.cpp:45
```

**How to read it:**
1. **Type**: "heap-use-after-free" - accessing freed memory
2. **READ at**: Where the invalid access happened
3. **freed by thread T1**: Which thread freed the memory
4. **allocated by thread T0**: Original allocation

---

## Environment Variables

### TSAN Options
```bash
# Increase history (more detailed reports, slower)
export TSAN_OPTIONS="history_size=7"

# Suppress known false positives (create a suppressions file)
export TSAN_OPTIONS="suppressions=tsan_suppressions.txt"

# Print all reports (don't stop at first)
export TSAN_OPTIONS="halt_on_error=0"

# Combined
export TSAN_OPTIONS="history_size=7:halt_on_error=0"
```

### ASAN Options
```bash
# Get more stack frames
export ASAN_OPTIONS="malloc_context_size=20"

# Continue after first error
export ASAN_OPTIONS="halt_on_error=0"

# Detect stack-use-after-return (slower but catches more bugs)
export ASAN_OPTIONS="detect_stack_use_after_return=1"
```

### Windows ASAN (set before running)
```powershell
$env:ASAN_OPTIONS="halt_on_error=0"
```

---

## Performance Notes

- **TSAN**: ~5-15x slowdown, ~5-10x memory overhead
- **ASAN**: ~2x slowdown, ~2x memory overhead

For faster iteration:
1. Use `--gtest_filter` to run only multi-threaded tests
2. Use RelWithDebInfo preset instead of Debug (better optimization)
3. Reduce thread count in tests if needed

---

## Common Patterns in Chess Engines

### Pattern 1: Shared Transposition Table
```cpp
// WRONG - data race on TT entry
TTEntry& entry = tt[hash % size];
entry.score = score;  // Thread A writes
// Thread B reads entry.score simultaneously

// FIX - use atomic or accept benign race
std::atomic<int16_t> score;
// Or document that TT races are benign (common in chess engines)
```

### Pattern 2: Best Move Updates
```cpp
// WRONG - race on bestMove
if (score > bestScore) {
    bestScore = score;  // Thread A
    bestMove = move;    // Thread A
}
// Thread B might read bestMove while Thread A is updating

// FIX - use mutex or atomic store
std::lock_guard<std::mutex> lock(mtx);
bestScore = score;
bestMove = move;
```

### Pattern 3: Search Statistics
```cpp
// WRONG - non-atomic increment
stats.nodes++;  // Multiple threads

// FIX - use atomic
std::atomic<uint64_t> nodes;
nodes.fetch_add(1, std::memory_order_relaxed);
```

---

## TSAN Suppressions (if needed)

Create `tsan_suppressions.txt`:
```
# Benign race in transposition table (intentional lock-free design)
race:TT::probe
race:TT::store

# Third-party library races (if any)
race:spdlog::*
```

Then run:
```bash
TSAN_OPTIONS="suppressions=tsan_suppressions.txt" ./cmake-build-wsl-debug-tsan/test/FrankyCPP_v1.6_Test
```

---

## Debugging Workflow

1. **Build with TSAN** (WSL recommended):
   ```bash
   cmake --preset wsl-debug-tsan
   cmake --build cmake-build-wsl-debug-tsan
   ```

2. **Run problematic test repeatedly**:
   ```bash
   for i in {1..10}; do
     echo "Run $i"
     ./cmake-build-wsl-debug-tsan/test/FrankyCPP_v1.6_Test --gtest_filter=SearchSmpTest.* 2>&1 | tee run_$i.log
   done
   ```

3. **Analyze reports**: Look for the first data race report

4. **Fix the race**: Add proper synchronization

5. **Verify fix**: Run again until no more reports

6. **Test on Windows**: Rebuild normally and verify crash is fixed

---

## IDE Integration (CLion)

1. **Add configurations**: File → Settings → Build → CMake
2. **Add new profile**: Click `+`, select one of the sanitizer presets
3. **Run with sanitizer**: Select the TSAN/ASAN profile and run tests
4. **View output**: Sanitizer reports appear in the Run window

---

## Known Limitations

- **TSAN + ASAN**: Cannot use together (mutually exclusive)
- **MSVC TSAN**: Not available - use WSL for threading analysis
- **Windows ASAN**: Works but less mature than GCC/Clang
- **Performance**: Sanitizers significantly slow down execution
- **False positives**: Some intentional lock-free patterns may trigger warnings

---

*Last updated: 2026-03-01*
