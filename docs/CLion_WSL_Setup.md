# CLion WSL Toolchain Setup Guide

**Date:** 2026-01-31  
**Project:** FrankyCPP v0.7  
**Purpose:** Configure CLion to build and debug using WSL (Ubuntu 22.04 with GCC 13)

---

## Prerequisites

Before starting, ensure you have:
- ✅ WSL build working (`./build_wsl.sh` succeeds)
- ✅ GCC 13 installed in WSL (`gcc --version` shows 13.x)
- ✅ vcpkg installed in WSL (`~/vcpkg` exists)
- ✅ VCPKG_ROOT set in WSL (`echo $VCPKG_ROOT` shows `~/vcpkg`)
- ✅ CLion 2021.3+ (supports WSL toolchains)

If any are missing, run:
```bash
./setup_linux_build_env.sh --install
source ~/.bashrc
```

---

## Step-by-Step Setup

### Step 1: Add WSL Toolchain

1. **Open CLion Settings**
   - `File` → `Settings` (or `Ctrl+Alt+S`)
   - Navigate to: `Build, Execution, Deployment` → `Toolchains`

2. **Add WSL Toolchain**
   - Click `+` button
   - Select `WSL`
   - Name it: `WSL Ubuntu (GCC 13)`

3. **Configure Toolchain**
   - **WSL Distribution:** Select `Ubuntu-22.04` (or your WSL distro)
   - CLion will auto-detect:
     - **CMake:** `/usr/bin/cmake` (version 3.22.1)
     - **Build Tool:** `/usr/bin/ninja` (or `/usr/bin/make`)
     - **C Compiler:** `/usr/bin/gcc` (version 13.x)
     - **C++ Compiler:** `/usr/bin/g++` (version 13.x)
     - **Debugger:** `/usr/bin/gdb`

4. **Set Environment Variables**
   - In the toolchain settings, find "Environment" section
   - Add environment variable:
     ```
     VCPKG_ROOT=/home/frank/vcpkg
     ```
   - Replace `frank` with your WSL username if different

5. **Move Toolchain Priority** (Optional but recommended)
   - Use arrow buttons to move WSL toolchain to top of list
   - This makes it the default for new profiles

6. **Click OK to save**

**Screenshot of what you should see:**
```
Toolchain: WSL Ubuntu (GCC 13)
├── CMake: /usr/bin/cmake (3.22.1)
├── Build Tool: /usr/bin/ninja (1.10.1)
├── C Compiler: /usr/bin/gcc (13.3.0)
├── C++ Compiler: /usr/bin/g++ (13.3.0)
├── Debugger: /usr/bin/gdb (12.1)
└── Environment: VCPKG_ROOT=/home/frank/vcpkg
```

---

### Step 2: Create CMake Profiles for WSL

1. **Open CMake Settings**
   - `File` → `Settings` → `Build, Execution, Deployment` → `CMake`

2. **Add WSL Debug Profile**
   - Click `+` button
   - **Name:** `WSL-Debug`
   - **Build type:** `Debug`
   - **Toolchain:** `WSL Ubuntu (GCC 13)` (from dropdown)
   - **CMake options:** `-DCMAKE_TOOLCHAIN_FILE=$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`
   - **Build directory:** `cmake-build-wsl-debug`
   - **Build options:** `-j 16` (adjust to your CPU cores)

3. **Add WSL Release Profile**
   - Click `+` button again
   - **Name:** `WSL-Release`
   - **Build type:** `Release`
   - **Toolchain:** `WSL Ubuntu (GCC 13)`
   - **CMake options:** `-DCMAKE_TOOLCHAIN_FILE=$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`
   - **Build directory:** `cmake-build-wsl-release`
   - **Build options:** `-j 16`

4. **Add WSL RelWithDebInfo Profile** (Optional)
   - Click `+` button
   - **Name:** `WSL-RelWithDebInfo`
   - **Build type:** `RelWithDebInfo`
   - **Toolchain:** `WSL Ubuntu (GCC 13)`
   - **CMake options:** `-DCMAKE_TOOLCHAIN_FILE=$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`
   - **Build directory:** `cmake-build-wsl-relwithdebinfo`
   - **Build options:** `-j 16`

5. **Click OK to save**

**Profile Summary:**
```
CMake Profiles:
├── Debug (MSVC) - Windows native
├── Release (MSVC) - Windows native
├── RelWithDebInfo (MSVC) - Windows native
├── MinSizeRel (MSVC) - Windows native
├── WSL-Debug (GCC 13) - Linux via WSL ← New
├── WSL-Release (GCC 13) - Linux via WSL ← New
└── WSL-RelWithDebInfo (GCC 13) - Linux via WSL ← New
```

---

### Step 3: Reload CMake Project

1. **Trigger CMake Reload**
   - Click reload icon in CMake tool window
   - Or: `Tools` → `CMake` → `Reload CMake Project`

2. **Wait for Configuration**
   - CLion will configure all profiles (including new WSL ones)
   - First time will take 5-10 minutes (vcpkg downloads dependencies)
   - Watch CMake output in bottom panel

3. **Verify Success**
   - Check CMake tool window for green checkmarks
   - WSL profiles should show: ✓ Configured successfully
   - If errors, see Troubleshooting section below

**Expected CMake Output:**
```
-- The C compiler identification is GNU 13.3.0
-- The CXX compiler identification is GNU 13.3.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/gcc-13 - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
...
-- Running vcpkg install
...
-- Configuring done
-- Generating done
-- Build files have been written to: /mnt/d/_DEV/FrankyCPP/cmake-build-wsl-debug
```

---

### Step 4: Build with WSL Profile

1. **Select Profile**
   - Use profile selector dropdown (top-right of main window)
   - Select: `WSL-Debug` or `WSL-Release`

2. **Build Project**
   - Click hammer icon (Build)
   - Or: `Build` → `Build Project` (Ctrl+F9)
   - Or: `Build` → `Rebuild Project` (Ctrl+Shift+F9)

3. **Monitor Build Progress**
   - Watch build output in bottom panel
   - First build: ~5-10 minutes
   - Subsequent builds: ~1-2 minutes

4. **Verify Success**
   - Check for "Build finished" message
   - Executable location: `cmake-build-wsl-debug/src/FrankyCPP_v0.7`

---

### Step 5: Run and Debug

#### Run Configuration

1. **Auto-Generated Run Configurations**
   - CLion creates these automatically:
     - `FrankyCPP_v1.5` (main executable)
     - `FrankyCPP_v1.5_Test` (test suite)
     - `All CTest` (run all tests via CTest)

2. **Select Run Configuration**
   - Use run configuration selector (top-right)
   - Choose: `FrankyCPP_v1.5 | WSL-Debug`

3. **Run Application**
   - Click green play button (Run)
   - Or: `Run` → `Run 'FrankyCPP_v1.5'` (Shift+F10)
   - Terminal opens in WSL context

#### Debug Configuration

1. **Set Breakpoints**
   - Click in gutter next to line numbers
   - Red dot appears

2. **Start Debugging**
   - Click green bug button (Debug)
   - Or: `Run` → `Debug 'FrankyCPP_v1.5'` (Shift+F9)

3. **Debug Features Available**
   - ✅ Step over (F8)
   - ✅ Step into (F7)
   - ✅ Step out (Shift+F8)
   - ✅ Evaluate expressions (Alt+F8)
   - ✅ Watch variables
   - ✅ Call stack navigation

**Note:** GDB in WSL provides full debugging support!

---

### Step 6: Run Tests

#### Option 1: Run All Tests
1. Select run config: `FrankyCPP_v1.5_Test | WSL-Debug`
2. Click Run or Debug button
3. Watch GTest output in console

#### Option 2: Run Specific Test
1. Open test file (e.g., `test/engine/SearchTest.cpp`)
2. Right-click on test function
3. Select: `Run 'TestName'` or `Debug 'TestName'`
4. CLion runs just that test

#### Option 3: CTest Integration
1. Select run config: `All CTest | WSL-Debug`
2. Click Run button
3. CTest runs all tests and shows results in tree view

---

## Troubleshooting

### Issue 1: "vcpkg toolchain not detected" / "Could not find spdlog"

**Symptom:**
```
CMake Warning: vcpkg toolchain not detected. Dependencies may not be found.
CMake Error: Could not find a package configuration file provided by "spdlog"
```

**Root Cause:**
CLion doesn't automatically pass the vcpkg toolchain file even though VCPKG_ROOT is set in the toolchain environment.

**Solution:**
1. Open CLion Settings → Build, Execution, Deployment → CMake
2. Find your WSL profile (e.g., WSL-Release)
3. In **CMake options** field, add:
   ```
   -DCMAKE_TOOLCHAIN_FILE=$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
   ```
4. Click OK
5. Reload CMake project (Tools → CMake → Reload CMake Project)

**Verification:**
CMake output should now show:
```
-- ✓ vcpkg toolchain detected: /home/frank/vcpkg/scripts/buildsystems/vcpkg.cmake
-- Running vcpkg install
...
```

**Note:** This is required for ALL WSL profiles (Debug, Release, RelWithDebInfo)

---

### Issue 2: "Cannot find vcpkg"

**Symptom:**
```
CMake Error: Could not find CMAKE_TOOLCHAIN_FILE
```

**Solution:**
1. Check VCPKG_ROOT in toolchain environment
2. Verify path in WSL: `wsl bash -c "ls -la ~/vcpkg"`
3. If using different location, update environment variable
4. Reload CMake project

### Issue 2: "GCC version too old"

**Symptom:**
```
CMake Error: GCC 13.0 or higher is required. Found: 11.4.0
```

**Solution:**
1. Install GCC 13: `./setup_linux_build_env.sh --install`
2. Verify: `wsl bash -c "gcc --version"`
3. Should show 13.x
4. Reload CMake project

### Issue 3: "Ninja not found"

**Symptom:**
```
CMake Error: Could not find Ninja
```

**Solution:**
1. Install Ninja: `wsl bash -c "sudo apt install ninja-build"`
2. Or set Build Tool to `/usr/bin/make` in toolchain
3. Reload CMake project

### Issue 4: vcpkg packages not installing

**Symptom:**
Build hangs at "Running vcpkg install" or shows errors

**Solution:**
1. Check network connectivity in WSL
2. Verify vcpkg is bootstrapped: `wsl bash -c "~/vcpkg/vcpkg version"`
3. Try manual install: `wsl bash -c "cd ~/vcpkg && ./vcpkg install boost-program-options"`
4. If still fails, check vcpkg logs in build directory

### Issue 5: Debugger not starting

**Symptom:**
Debug button does nothing or shows error

**Solution:**
1. Verify GDB installed: `wsl bash -c "gdb --version"`
2. If missing: `wsl bash -c "sudo apt install gdb"`
3. Check WSL firewall/antivirus not blocking
4. Try rebuilding in Debug mode first

### Issue 6: "Permission denied" errors

**Symptom:**
Cannot write to build directory or run executables

**Solution:**
1. Check Windows file permissions on D:\_DEV\FrankyCPP
2. Try cleaning build: `wsl bash -c "rm -rf cmake-build-wsl-*"`
3. Rebuild from scratch
4. If persists, check WSL mount options

### Issue 7: CMake preset conflicts

**Symptom:**
```
CMake Error: Preset 'linux-debug' not found
```

**Solution:**
This is normal - CLion doesn't use CMakePresets.json by default
- CLion creates its own build directories
- Profiles work independently of presets
- You can ignore preset-related messages in CLion

---

## Tips & Best Practices

### 1. Use Separate Profiles for Windows and WSL
- **Windows profiles:** Use for MSVC-specific work
- **WSL profiles:** Use for Linux compatibility testing
- **Both:** Share same source code, different binaries

### 2. Switching Between Profiles
- Change profile selector dropdown (top-right)
- CLion rebuilds only if needed
- Build artifacts are separate (no conflicts)

### 3. Git Integration
- Works seamlessly with both Windows and WSL profiles
- `.gitignore` excludes both `cmake-build-*` directories
- Commit from either Windows or WSL (same result)

### 4. Performance Optimization
- **First build:** Always slow (vcpkg dependencies)
- **Incremental builds:** Fast (~1-2 min)
- **Tip:** Use Ninja (faster than Make)
- **Tip:** Increase `-j` value (more parallel jobs)

### 5. Debugging Performance
- **Debug builds:** No optimization, easier debugging
- **RelWithDebInfo:** Optimized but debuggable
- **Release:** Fully optimized, hard to debug
- **Tip:** Use Debug for development, RelWithDebInfo for profiling

### 6. Terminal Integration
- **WSL Terminal in CLion:** `Tools` → `Start SSH Session` → `WSL`
- Or use Windows Terminal with WSL tab
- Navigate to: `cd /mnt/d/_DEV/FrankyCPP`

### 7. File Sync
- CLion automatically syncs files between Windows and WSL
- No manual sync needed
- Changes in Windows immediately visible in WSL

---

## Verification Checklist

After setup, verify everything works:

- [ ] **Toolchain detected:** WSL toolchain shows GCC 13, CMake, Ninja
- [ ] **VCPKG_ROOT set:** Environment variable in toolchain config
- [ ] **Profiles created:** WSL-Debug, WSL-Release visible
- [ ] **CMake configured:** All WSL profiles show green checkmarks
- [ ] **Build succeeds:** WSL-Debug and WSL-Release both build
- [ ] **Tests run:** All 266 tests pass in WSL profile
- [ ] **Debugger works:** Can set breakpoints and step through code
- [ ] **Run configs exist:** FrankyCPP_v0.7 and tests available

If all checked, setup is complete! ✅

---

## Quick Reference

### Common Actions

| Action | Windows Shortcut | Menu Path |
|--------|------------------|-----------|
| **Build Project** | Ctrl+F9 | Build → Build Project |
| **Rebuild Project** | Ctrl+Shift+F9 | Build → Rebuild Project |
| **Run** | Shift+F10 | Run → Run |
| **Debug** | Shift+F9 | Run → Debug |
| **Stop** | Ctrl+F2 | Run → Stop |
| **Settings** | Ctrl+Alt+S | File → Settings |
| **Reload CMake** | - | Tools → CMake → Reload CMake Project |

### Useful CLion Features for WSL

- **Compare builds:** Right-click build directory → Compare With
- **Clean build:** Right-click profile → Clean
- **Terminal:** Alt+F12 (can switch to WSL)
- **CMake tool window:** View → Tool Windows → CMake
- **Build tool window:** View → Tool Windows → Build

---

## Alternative: Using CMakePresets.json

If you prefer using CMakePresets.json instead of CLion profiles:

1. **Enable CMake Presets**
   - `File` → `Settings` → `Build, Execution, Deployment` → `CMake`
   - Check: `Enable CMake Presets`
   - Click: `Reload CMake Project`

2. **CLion will detect presets:**
   - `linux-debug`
   - `linux-release`
   - `linux-relwithdebinfo`

3. **Profiles auto-created from presets**
   - CLion creates profiles for each preset
   - Use toolchain: WSL Ubuntu (GCC 13)

**Note:** This is optional. CLion profiles work great without presets.

---

## Summary

**Setup Time:** ~10-15 minutes  
**Result:** Full WSL development in CLion with:
- ✅ GCC 13 compiler
- ✅ Full C++20 support
- ✅ Integrated debugging
- ✅ Parallel builds
- ✅ All tests passing
- ✅ Same codebase, multiple platforms

**You can now:**
- Build for Windows (MSVC) and Linux (GCC 13) in same IDE
- Debug both platforms with full IDE support
- Run tests from CLion test runner
- Switch between platforms with dropdown selector

---

## Next Steps

After CLion setup:
1. ✅ Try building with WSL-Debug profile
2. ✅ Run test suite in WSL
3. ✅ Set a breakpoint and debug
4. ✅ Compare Windows vs Linux builds
5. 🎯 Start developing with multi-platform confidence!

---

*Setup guide created: 2026-01-31*  
*Tested with: CLion 2024.1, WSL Ubuntu 22.04, GCC 13.3*  
*Ready for: Production development*
