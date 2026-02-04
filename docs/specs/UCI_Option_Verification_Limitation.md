# UCI Option Verification Limitation - Important Discovery

**Date:** 2026-02-04

---

## Summary

While implementing `getOptions()` for UCIEngine, we discovered a fundamental limitation of the UCI protocol: **there is no standard way to verify that UCI options were actually applied by the engine.**

This is not a bug in our implementation - it's a limitation of the UCI protocol itself.

---

## The Problem

### UCI Protocol Gap

The UCI protocol specification provides:
- ✅ `setoption name <name> value <value>` - Command to set options
- ✅ `isready` / `readyok` - Synchronization
- ❌ **No error reporting if option setting fails**
- ❌ **No way to query current option values**

### What This Means

When you send `setoption name Hash value 512`:
1. Engine receives the command
2. Engine may or may not accept it (invalid value? unrecognized option?)
3. Engine does NOT report success or failure
4. Engine just responds to next `isready` with `readyok`
5. **No way to know if the option was actually applied**

---

## Example: Stockfish

```
>>> setoption name Hash value 999999999  ← Invalid value (way too large)
>>> isready
<<< readyok                              ← No error! Looks like it worked!

>>> uci                                  ← Try to check if it was applied
<<< option name Hash type spin default 16 min 1 max 33554432
<<< uciok

The "default" field still shows 16!
But we can't tell if:
  a) The option was rejected (engine kept default)
  b) The option was accepted (but default field not updated)
```

**There is no way to know what the current Hash value is!**

---

## FrankyCPP's Solution: `current` Field

FrankyCPP extends the UCI protocol with a non-standard `current` field:

```
>>> uci
<<< option name Hash type spin default 64 min 0 max 4096 current 64
                                ^^                       ^^
                                Initial default          Current value
                                (never changes)          (reflects changes!)

>>> setoption name Hash value 512
>>> isready
<<< readyok

>>> uci
<<< option name Hash type spin default 64 min 0 max 4096 current 512
                                                          ^^^^^^^^^^^
                                                          Updated to 512!
```

**This is a FrankyCPP-specific extension for debugging.**
- Not part of UCI standard
- Other engines don't have this
- Extremely useful for development/testing

---

## Our Implementation

### getOptions() Strategy

We implemented a **two-stage parsing strategy**:

```cpp
// STEP 1: Try to find "current" field (FrankyCPP extension)
if (line has "current") {
  return value from "current" field;  // Accurate!
}

// STEP 2: Fall back to "default" field (UCI standard)
else if (line has "default") {
  return value from "default" field;  // May be stale for non-FrankyCPP!
}
```

### Behavior

**With FrankyCPP:**
```cpp
engine.setUciOptions("Hash=512");
auto opts = engine.getOptions();
assert(opts["Hash"] == "512");  // ✅ Passes! Uses "current" field
```

**With Stockfish:**
```cpp
engine.setUciOptions("Hash=512");
auto opts = engine.getOptions();
// opts["Hash"] might still be "16" (original default)
// Cannot verify if 512 was actually applied!
```

---

## Practical Implications

### For FrankyCPP Testing

✅ **Can verify options were applied:**
```cpp
TEST_F(Test, VerifyOptions) {
  engine.setUciOptions("Hash=256; OwnBook=false");
  auto opts = engine.getOptions();
  EXPECT_EQ(opts["Hash"], "256");       // Works!
  EXPECT_EQ(opts["OwnBook"], "false");  // Works!
}
```

### For Other Engines

⚠️ **Cannot verify options were applied:**
```cpp
TEST_F(Test, OtherEngine) {
  engine.setUciOptions("Hash=512");
  auto opts = engine.getOptions();
  // opts["Hash"] may return original default, not 512
  // Must trust that engine accepted the value
}
```

**Alternative: Use debug mode to see UCI communication:**
```cpp
engine.setDebugMode(true);
engine.setUciOptions("Hash=512");
// Watch console output:
// [UCIEngine] >>> setoption name Hash value 512
// [UCIEngine] >>> isready
// [UCIEngine] <<< readyok
// If you see readyok, assume it worked (no other way to verify)
```

---

## Recommendations

### When Testing FrankyCPP

✅ Use `getOptions()` to verify configuration:
```cpp
auto opts = engine.getOptions();
ASSERT_EQ(opts["Hash"], "512");
ASSERT_EQ(opts["OwnBook"], "false");
```

### When Testing Other Engines

1. **Enable debug mode** to see UCI communication
2. **Trust `readyok`** response (best we can do)
3. **Test behavior indirectly** (e.g., does engine actually search without book?)
4. **Don't expect `getOptions()` to reflect changes**

### In Production

For test suites comparing engines:
```yaml
testSuites:
  - name: "franky_tests"
    enginePath: "engine.exe"
    debugMode: false          # Assume options work
    uciOptions: "OwnBook=false; Hash=256"
```

If configuration is critical:
```yaml
testSuites:
  - name: "critical_test"
    enginePath: "engine.exe"
    debugMode: true           # Enable to verify UCI communication
    uciOptions: "OwnBook=false"
```

---

## Why UCI Doesn't Have This

The UCI protocol was designed for simplicity:
- Engines implement what they support
- GUIs send options they want
- If engine doesn't recognize option, it silently ignores it
- No error handling needed

This works for GUIs because:
- GUIs query available options via `uci` command at startup
- GUIs only send options they know engine supports
- GUIs don't need to verify (they trust engine)

But for testing/debugging:
- We want to verify configuration
- We want to detect invalid values
- We want to compare expected vs actual

**FrankyCPP's `current` field solves this for FrankyCPP engines!**

---

## Conclusion

### What We Learned

1. **UCI Protocol Limitation**: No standard way to verify options were applied
2. **FrankyCPP Extension**: `current` field provides accurate current values
3. **Other Engines**: Must trust `readyok` response, cannot verify

### Our Solution

`getOptions()` implementation:
- ✅ Works perfectly with FrankyCPP (uses `current` field)
- ⚠️ Limited with other engines (falls back to `default` field)
- ✅ Compatible with all UCI engines
- ✅ Documented limitations clearly

### Documentation Updated

- ✅ `UCIEngine.h` - Method documentation explains limitation
- ✅ `UCIEngine.cpp` - Code comments explain two-stage parsing
- ✅ Tests - Updated with notes about FrankyCPP-specific behavior
- ✅ Spec doc - Detailed explanation of limitation and solution

---

**Status:** Implementation complete with full understanding of UCI limitation ✅

*Date: 2026-02-04*
*Discovery: UCI protocol has no standard option verification mechanism*
*Solution: FrankyCPP's `current` field extension + two-stage parsing*
