#!/usr/bin/bash

#
# FrankyCPP
# Copyright (c) 2018-2026 Frank Kopp
#
# MIT License
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

set -e  # Exit on error

# Parse arguments
BUILD_MODE="release"
COMPILER="gcc"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        debug|release|relwithdebinfo)
            BUILD_MODE="$1"
            shift
            ;;
        gcc|clang)
            COMPILER="$1"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [BUILD_MODE] [COMPILER]"
            echo ""
            echo "BUILD_MODE: debug, release, relwithdebinfo (default: release)"
            echo "COMPILER:   gcc, clang (default: gcc)"
            echo ""
            echo "Examples:"
            echo "  $0                    # Release build with GCC"
            echo "  $0 debug              # Debug build with GCC"
            echo "  $0 release clang      # Release build with Clang"
            echo "  $0 debug clang        # Debug build with Clang"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "=========================================="
echo "FrankyCPP v0.7 - Linux/WSL Build Script"
echo "Build Mode: $BUILD_MODE"
echo "Compiler: $COMPILER"
echo "=========================================="

# Validate VCPKG_ROOT
if [ -z "$VCPKG_ROOT" ]; then
    echo "ERROR: VCPKG_ROOT is not set"
    echo "Set it with: export VCPKG_ROOT=~/vcpkg"
    exit 1
fi

echo "Using vcpkg from: $VCPKG_ROOT"

# Enable parallel builds for vcpkg (speeds up Boost compilation significantly)
CPU_CORES=$(nproc)
export VCPKG_MAX_CONCURRENCY=$CPU_CORES
echo "Enabling vcpkg parallel builds: $CPU_CORES cores"
echo ""

# Determine preset based on compiler
if [ "$COMPILER" = "clang" ]; then
    PRESET="wsl-clang-$BUILD_MODE"
else
    PRESET="wsl-$BUILD_MODE"
fi

# Configure using CMake preset
echo "Configuring with preset: $PRESET"
cmake --preset "$PRESET"

# Build
if [ "$COMPILER" = "clang" ]; then
    BUILD_DIR="cmake-build-wsl-clang-$BUILD_MODE"
else
    BUILD_DIR="cmake-build-wsl-$BUILD_MODE"
fi
echo "Building in: $BUILD_DIR"
cmake --build "$BUILD_DIR" --parallel

# Run tests (excluding slow tests)
echo ""
echo "Running tests..."
# Find test executable (version-independent)
TEST_EXE=$(find "./$BUILD_DIR/test" -name "FrankyCPP_v*_Test" -type f 2>/dev/null | head -n 1)
if [ -n "$TEST_EXE" ] && [ -f "$TEST_EXE" ]; then
    "$TEST_EXE" --gtest_filter=-*SpeedTests.*:*TimingTests.*
else
    echo "WARNING: Test executable not found in ./$BUILD_DIR/test/"
fi

# Show engine executable location (version-independent)
EXE_PATH=$(find "./$BUILD_DIR/src" -name "FrankyCPP_v*" -type f ! -name "*.o" ! -name "*.d" 2>/dev/null | head -n 1)
# Show engine executable location (version-independent)
EXE_PATH=$(find "./$BUILD_DIR/src" -name "FrankyCPP_v*" -type f ! -name "*.o" ! -name "*.d" 2>/dev/null | head -n 1)
if [ -n "$EXE_PATH" ] && [ -f "$EXE_PATH" ]; then
    echo ""
    echo "=========================================="
    echo "Build successful!"
    echo "Engine: $EXE_PATH"
    echo "=========================================="
else
    echo "WARNING: Engine executable not found in ./$BUILD_DIR/src/"
fi
