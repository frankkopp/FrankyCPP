# How to Add GitHub Actions Artifacts to v1.0.0 Release

## Recommended Method: Automated Artifact Upload

The best way is to **modify the workflow** to automatically upload artifacts to releases. However, since v1.0.0 is already tagged, here's the manual approach.

## Option 1: Manual Method (For v1.0.0)

### Step 1: Download Artifacts from Actions

1. Go to https://github.com/frankkopp/FrankyCPP/actions
2. Find the successful "CI Build" run on **master** branch (commit `8eeae10`)
3. Scroll to **"Artifacts"** section at bottom
4. Download:
   - `frankycpp-windows-Release.zip`
   - `frankycpp-linux-gcc-Release.zip`

### Step 2: Create GitHub Release

1. Go to: https://github.com/frankkopp/FrankyCPP/releases/new
2. Fill in the form:

**Choose a tag:** `v1.0.0` (should already exist)

**Release title:** `FrankyCPP v1.0.0 - Production Release`

**Description:** Copy from `RELEASE_NOTES_v1.0.0.md` or use this:

```markdown
# FrankyCPP v1.0.0 - Production Release 🎉

FrankyCPP is now production-ready with complete cross-platform support and professional infrastructure.

## Highlights

- ✅ **Cross-platform:** Windows (MSVC 2022), Linux (GCC 13, Clang 18), macOS-ready
- ✅ **Modern C++20:** Full feature usage including std::format
- ✅ **Comprehensive testing:** 266+ unit tests, all passing on all platforms
- ✅ **CI/CD:** GitHub Actions validates every push across all platforms
- ✅ **Professional infrastructure:** Complete documentation and automated builds

## Features

- **UCI chess protocol** - Full implementation for GUI compatibility
- **Alpha-beta search** - Advanced pruning techniques and optimizations
- **Configurable evaluation** - YAML-based runtime configuration
- **Opening book support** - PGN format parsing and caching
- **Cross-compiler validated** - MSVC, GCC 13, and Clang 18

## Download

See **Assets** below for pre-built binaries:
- Windows (MSVC 2022) - Extract and run `FrankyCPP_v1.0.exe`
- Linux (GCC 13) - Extract and run `FrankyCPP_v1.0`

## Building from Source

See [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md) for complete instructions.

**Quick start:**
```bash
# Windows
.\build_windows.ps1 release

# Linux
./build_wsl.sh release gcc
```

## What's Next (v1.x Roadmap)

See [docs/V1.0_RELEASE_READINESS.md](docs/V1.0_RELEASE_READINESS.md) for details:
- Lazy SMP multi-threading
- NNUE evaluation
- Syzygy tablebase support
- Performance optimizations

## Full Changelog

**v0.7 → v1.0.0 accomplishments:**
- ✅ Professional build infrastructure with CMake and vcpkg
- ✅ GitHub Actions CI/CD deployment (Windows, Linux GCC, Linux Clang)
- ✅ Comprehensive documentation (8 guides covering all aspects)
- ✅ Complete cross-platform support (Windows/Linux working, macOS-ready)
- ✅ Clang 18 support with full C++20 compatibility
- ✅ YAML-based configuration system overhaul
- ✅ License compliance (MIT) - removed GPL references
- ✅ 266+ unit tests with full coverage
- ✅ Automated setup scripts for all platforms

**Technical improvements:**
- Modern C++20 codebase (std::format, constexpr, concepts)
- Precompiled headers for faster builds
- LTO/IPO for optimized Release builds
- Sanitizer support (ASan, UBSan) for Debug builds
- CMake presets for IDE integration

---

**Ready for production use!** 🚀

Play chess with FrankyCPP using any UCI-compatible chess GUI.
```

### Step 4: Attach the Artifacts

1. Scroll down to the **"Attach binaries"** section
2. Drag and drop or click to upload:
   - `frankycpp-windows-Release.zip` (downloaded from Actions)
   - `frankycpp-linux-gcc-Release.zip` (downloaded from Actions)

**Optional:** Rename the files before uploading for clarity:
   - `frankycpp-windows-Release.zip` → `FrankyCPP-v1.0.0-Windows-MSVC.zip`
   - `frankycpp-linux-gcc-Release.zip` → `FrankyCPP-v1.0.0-Linux-GCC13.zip`

### Step 5: Publish the Release

1. Check the **"Set as the latest release"** checkbox
2. Click **"Publish release"** button

---

## Option 2: Automated Workflow (For Future Releases)

**Better approach:** Modify `.github/workflows/ci-build.yml` to automatically upload artifacts to releases.

Add this job to your workflow:

```yaml
  release:
    name: Upload Release Artifacts
    runs-on: ubuntu-latest
    needs: [build-windows, build-linux]
    if: startsWith(github.ref, 'refs/tags/v')
    
    steps:
      - name: Download Windows artifacts
        uses: actions/download-artifact@v4
        with:
          name: frankycpp-windows-Release
          path: windows-release
      
      - name: Download Linux artifacts
        uses: actions/download-artifact@v4
        with:
          name: frankycpp-linux-gcc-Release
          path: linux-release
      
      - name: Create Release Archives
        run: |
          cd windows-release
          zip -r ../FrankyCPP-${{ github.ref_name }}-Windows-MSVC.zip *
          cd ../linux-release
          tar czf ../FrankyCPP-${{ github.ref_name }}-Linux-GCC13.tar.gz *
      
      - name: Upload to Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            FrankyCPP-${{ github.ref_name }}-Windows-MSVC.zip
            FrankyCPP-${{ github.ref_name }}-Linux-GCC13.tar.gz
```

**How it works:**
1. Triggered automatically when you push a tag (e.g., `v1.0.0`)
2. Downloads artifacts from Windows and Linux build jobs
3. Creates properly named release archives
4. Uploads them directly to the GitHub Release

**For v1.1.0 and beyond:** Just push a tag and artifacts are automatically attached!

---

## Option 3: Using GitHub CLI

If you have GitHub CLI (`gh`) installed:

```bash
# Navigate to repository
cd D:\_DEV\FrankyCPP

# Download artifacts from latest master workflow run
gh run list --branch master --limit 1
gh run download <run-id>  # Replace with actual run ID

# Create release with artifacts
gh release create v1.0.0 \
  --title "FrankyCPP v1.0.0 - Production Release" \
  --notes-file RELEASE_NOTES_v1.0.0.md \
  frankycpp-windows-Release.zip \
  frankycpp-linux-gcc-Release.zip
```

---

## Verification

After publishing, verify:

1. Release page: https://github.com/frankkopp/FrankyCPP/releases/tag/v1.0.0
2. Check that artifacts are attached and downloadable
3. Test download and extraction on each platform
4. Verify executables run correctly

---

## Direct Links (After Release Creation)

- **Repository:** https://github.com/frankkopp/FrankyCPP
- **Releases:** https://github.com/frankkopp/FrankyCPP/releases
- **Latest Release:** https://github.com/frankkopp/FrankyCPP/releases/latest
- **v1.0.0 Release:** https://github.com/frankkopp/FrankyCPP/releases/tag/v1.0.0

---

## Troubleshooting

### Artifacts Not Found

If artifacts aren't available:
1. Check that the workflow completed successfully
2. Artifacts might have expired (90-day default)
3. You can trigger a new workflow run:
   - Go to Actions tab
   - Select "CI Build" workflow
   - Click "Run workflow" → Select "master" branch

### Wrong Artifacts

If you need to replace artifacts:
1. Edit the release
2. Delete old artifacts
3. Upload new artifacts
4. Save changes

---

**Ready to create your release!** 🚀
