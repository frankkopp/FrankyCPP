# GitHub Actions CI - Next Steps

## Current Status: Ready for Testing ⏭️

The CI workflow file is ready but untested: `.github/workflows/ci-build.yml`

---

## Expected CI Issues & Solutions

### Issue 1: Clang 18 Not Available on Ubuntu Runners

**Symptom:** `clang-18: command not found`

**Cause:** GitHub's ubuntu-latest (24.04) may not have Clang 18 in default repositories

**Solution:** The setup script adds LLVM repository:
```yaml
- name: Setup build environment
  run: |
    chmod +x ./setup_linux_build_env.sh
    sudo -E bash -c "cd $(pwd) && ./setup_linux_build_env.sh --install"
```

**Verify:** Check setup script output for:
```
Adding LLVM repository...
✓ Clang installed
```

---

### Issue 2: vcpkg Build Times

**Symptom:** First CI run takes 30+ minutes

**Cause:** vcpkg compiles Boost and other dependencies from source

**Solution (Future):** Enable vcpkg binary caching
```yaml
- name: Setup vcpkg cache
  uses: actions/cache@v3
  with:
    path: |
      /opt/vcpkg/downloads
      /opt/vcpkg/packages
      /opt/vcpkg/buildtrees
    key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}
```

**Current:** Accept slow first build, subsequent builds should be faster

---

### Issue 3: Test Discovery Timeout

**Symptom:** GoogleTest discovery step times out

**Cause:** Test executable taking too long to enumerate tests

**Solution:** Increase timeout in CMakeLists.txt:
```cmake
gtest_discover_tests(FrankyCPP_v0.7_Test
    DISCOVERY_TIMEOUT 60  # Increase from default 5 seconds
)
```

---

### Issue 4: Windows MSVC Environment

**Symptom:** MSVC compiler not found or environment not initialized

**Cause:** Need to properly initialize MSVC environment

**Solution:** Already using `microsoft/setup-msbuild@v2` action
```yaml
- name: Setup MSVC
  uses: microsoft/setup-msbuild@v2
```

**Verify:** Check for "MSVC environment detected" in logs

---

### Issue 5: Artifact Upload Path Issues

**Symptom:** Artifacts not found or empty

**Cause:** Executable paths don't match patterns

**Current patterns:**
```yaml
path: |
  build/src/FrankyCPP_*.exe
  build/test/FrankyCPP_*_Test.exe
```

**May need to adjust** based on actual build output locations

---

### Issue 6: Environment Variable Propagation

**Symptom:** Setup script validation fails, thinking it's not in CI

**Cause:** `CI` environment variable not set or passed correctly

**Solution:** Explicitly set in workflow:
```yaml
env:
  CI: true
  GITHUB_ACTIONS: true
```

**Verify:** Check setup script output for "CI environment detected"

---

## Testing Checklist

### Before Push
- [x] All local builds working (Windows, GCC, Clang)
- [x] All tests passing locally
- [x] Documentation updated
- [x] Changes committed

### After Push (Monitor in GitHub Actions)

#### Windows Build
- [ ] MSVC environment initialized
- [ ] vcpkg setup completes
- [ ] Release build compiles
- [ ] Tests run successfully
- [ ] Artifacts uploaded

#### Linux GCC Build
- [ ] GCC 13 installed
- [ ] vcpkg setup completes
- [ ] Release build compiles
- [ ] Tests run successfully
- [ ] Artifacts uploaded

#### Linux Clang Build
- [ ] LLVM repository added
- [ ] Clang 18 installed
- [ ] GCC 13 available (for libstdc++)
- [ ] vcpkg setup completes
- [ ] Release build compiles
- [ ] Tests run successfully

---

## Quick Fixes Reference

### If Clang 18 Install Fails
```bash
# Fallback to Clang 17
sudo apt install clang-17

# Update CMakePresets.json and ci-build.yml to use clang-17
```

### If LTO Issues Reappear
```cmake
# Further disable LTO
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
```

### If Test Discovery Times Out
```cmake
# In test/CMakeLists.txt
set(DISCOVERY_TIMEOUT 120)
gtest_discover_tests(... DISCOVERY_TIMEOUT ${DISCOVERY_TIMEOUT})
```

### If Artifacts Not Found
```yaml
# More specific paths
path: |
  cmake-build-*/src/FrankyCPP_v0_7*
  cmake-build-*/test/FrankyCPP_v0_7_Test*
```

---

## Monitoring Commands

### Check Workflow Status
```bash
# Via GitHub CLI (if installed)
gh run list --workflow=ci-build.yml

# Via web browser
https://github.com/frankkopp/FrankyCPP/actions
```

### Download Logs
```bash
gh run download <run-id>
```

### Cancel Failed Run
```bash
gh run cancel <run-id>
```

---

## Success Criteria

### All 3 Jobs Must Pass
1. ✅ Windows Release (with artifacts)
2. ✅ Linux GCC Release (with artifacts)
3. ✅ Linux Clang Release (validation only)

### Artifacts Must Be Available
- `frankycpp-windows-Release.zip`
- `frankycpp-linux-gcc-Release.tar.gz`

### Build Times (First Run)
- Windows: ~20-30 min (vcpkg + build)
- Linux GCC: ~15-25 min (vcpkg + build)
- Linux Clang: ~15-25 min (vcpkg + build)

### Build Times (Cached)
- All platforms: ~5-10 min (build only)

---

## Ready to Deploy

```bash
# Push to GitHub
git push origin dev_v0.7

# Watch the build
# Go to: https://github.com/frankkopp/FrankyCPP/actions
```

**Everything is prepared and ready for CI testing!** 🚀
