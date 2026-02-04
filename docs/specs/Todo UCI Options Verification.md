# UCI Options Verification Approach ✅

## Summary

Documented the approach to **verify UCI options were applied** by sending the `uci` command again and parsing the engine's response.

---

## How UCI Option Verification Works

### UCI Protocol Behavior

When you send the `uci` command to a UCI engine, it responds with:

```
id name <engine name>
id author <author>
option name <name1> type <type> default <value> [min <min>] [max <max>] [var <option1>]...
option name <name2> type <type> default <value> ...
...
uciok
```

**Key Point:** The `default <value>` field shows the **current value** of the option, not just the initial default!

### Example Flow

```
>>> uci
<<< id name FrankyCPP v1.1
<<< option name OwnBook type check default true
<<< option name Hash type spin default 128 min 1 max 65536
<<< uciok

>>> setoption name OwnBook value false
>>> isready
<<< readyok

>>> setoption name Hash value 512
>>> isready
<<< readyok

>>> uci                                      ← Send uci again!
<<< id name FrankyCPP v1.1
<<< option name OwnBook type check default false    ← Changed to false!
<<< option name Hash type spin default 512 min 1 max 65536  ← Changed to 512!
<<< uciok
```

---

## Implementation Approach

### Current State
✅ **Setting options works** - `setUciOptions()` sends setoption commands
❌ **Verification not implemented** - No way to confirm options were applied

### Future Enhancement: `getOptions()` Method

```cpp
class UCIEngine {
public:
  // Existing methods
  void setUciOptions(const std::string& options);
  
  // NEW: Query current option values
  std::map<std::string, std::string> getOptions();
};
```

**Implementation:**
```cpp
std::map<std::string, std::string> UCIEngine::getOptions() {
  std::map<std::string, std::string> options;
  
  // Send uci command
  sendCommand("uci");
  
  // Read response until uciok
  const auto deadline = steady_clock::now() + milliseconds(5000);
  while (steady_clock::now() < deadline) {
    std::string line;
    if (!readLine(line, milliseconds(1000))) break;
    
    // Parse: "option name <name> type <type> default <value> ..."
    if (line.rfind("option name ", 0) == 0) {
      // Extract name and current value
      size_t nameStart = 12; // After "option name "
      size_t typePos = line.find(" type ", nameStart);
      size_t defaultPos = line.find(" default ", typePos);
      
      if (typePos != std::string::npos && defaultPos != std::string::npos) {
        std::string name = line.substr(nameStart, typePos - nameStart);
        size_t valueStart = defaultPos + 9; // After " default "
        size_t valueEnd = line.find(' ', valueStart);
        std::string value = (valueEnd == std::string::npos) 
                          ? line.substr(valueStart)
                          : line.substr(valueStart, valueEnd - valueStart);
        
        options[name] = value;
      }
    }
    
    if (line.find("uciok") != std::string::npos) break;
  }
  
  return options;
}
```

**Usage:**
```cpp
// Set options
engine.setUciOptions("OwnBook=false; Hash=512");

// Verify they were applied
auto options = engine.getOptions();
EXPECT_EQ(options["OwnBook"], "false");
EXPECT_EQ(options["Hash"], "512");
```

---

## Test Documentation

### Test 21: Verify Options Applied (Documented)

Added test that **documents the verification approach** but is currently skipped:

```cpp
TEST_F(UCIEngineOptionsTest, VerifyOptionsApplied_SendUciAgain) {
  // NOTE: This test demonstrates that we CAN verify options were applied
  // by sending "uci" again and parsing the "option" lines in the response.
  
  GTEST_SKIP() << "Test demonstrates verification approach but not fully implemented";
  
  // Pseudocode for future implementation:
  // 1. engine.setUciOptions("OwnBook=false; Hash=256");
  // 2. auto options = engine.getOptions(); // Send "uci" and parse response
  // 3. EXPECT_EQ(options["OwnBook"], "false");
  // 4. EXPECT_EQ(options["Hash"], "256");
}
```

---

## Benefits of Verification

### ✅ Confirms Options Were Applied
- Engine actually changed the setting
- Not just silently ignored

### ✅ Catches Invalid Options
- If option name is wrong, engine keeps default value
- Verification would detect this

### ✅ Debugging Aid
- Can dump all current options
- Compare expected vs actual values

### ✅ Test Reliability
- Integration tests can verify correct configuration
- Catch configuration errors early

---

## When to Use Verification

### Always Verify When:
- ✅ Testing critical configurations
- ✅ Debugging why engine behaves differently than expected
- ✅ Validating option names are correct
- ✅ Writing integration tests

### Skip Verification When:
- ✅ Performance is critical (extra round-trip)
- ✅ Using well-known standard options (Hash, Threads)
- ✅ Engine is trusted (same version, tested before)

---

## Current Implementation

### What Works Now
✅ **Setting options** - `setUciOptions()` sends commands
✅ **Debug mode** - See UCI communication
✅ **Error handling** - Graceful failures

### Future Enhancement
❌ **Verification** - `getOptions()` not implemented yet
- Would require parsing "option" lines
- Would return map of current option values
- Useful for testing and debugging

### Workaround for Now
**Use debug mode to manually verify:**
```cpp
engine.setDebugMode(true);
engine.setUciOptions("OwnBook=false; Hash=512");

// Look at console output:
// [UCIEngine] >>> setoption name OwnBook value false
// [UCIEngine] >>> isready
// [UCIEngine] <<< readyok
// [UCIEngine] >>> setoption name Hash value 512
// [UCIEngine] >>> isready
// [UCIEngine] <<< readyok

// If you see "readyok" after each option, it was accepted
```

---

## Files Modified

1. ✅ `test/engine_arena/UCIEngine_OptionsTest.cpp` - Added Test 21 (skipped, documenting approach)
2. ✅ `src/engine_arena/UCIEngine.h` - Added documentation comment about verification

---

## Summary

**Verification Approach:** Send `uci` command again and parse `option` lines to see current values

**Current Status:**
- ✅ Documented in Test 21
- ✅ Commented in UCIEngine.h
- ❌ Not fully implemented (future enhancement)

**Workaround:** Use `debugMode=true` to see UCI communication and verify options manually

**When Needed:** Full implementation of `getOptions()` method would be valuable for integration tests and debugging

---

*Documentation Date: 2026-02-04*
*Feature: UCI Options Verification Approach*
*Status: Documented, not implemented ✅*
