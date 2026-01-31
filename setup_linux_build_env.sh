#!/bin/bash -l

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

# Source bashrc to load environment variables like VCPKG_ROOT
if [ -f ~/.bashrc ]; then
    source ~/.bashrc 2>/dev/null || true
fi

# Fallback: if VCPKG_ROOT is still not set, try common locations
if [ -z "$VCPKG_ROOT" ]; then
    if [ -d "$HOME/vcpkg" ]; then
        export VCPKG_ROOT="$HOME/vcpkg"
    elif [ -d "$HOME/.vcpkg-clion/vcpkg" ]; then
        export VCPKG_ROOT="$HOME/.vcpkg-clion/vcpkg"
    elif [ -d "/opt/vcpkg" ]; then
        export VCPKG_ROOT="/opt/vcpkg"
    fi
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse command line arguments
MODE="validate"  # Default: validate only (safe, no system modifications)
if [ "$1" = "--install" ] || [ "$1" = "-i" ]; then
    MODE="install"
elif [ "$1" = "--validate" ] || [ "$1" = "-v" ]; then
    MODE="validate"
elif [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "FrankyCPP Linux Build Environment Setup"
    echo ""
    echo "Usage:"
    echo "  $0              Validate environment (safe, no modifications)"
    echo "  $0 --install    Install dependencies and validate"
    echo "  $0 --validate   Validate environment only (same as default)"
    echo "  $0 --help       Show this help message"
    echo ""
    echo "SAFETY NOTE:"
    echo "  Default behavior is validate-only to avoid unintended system modifications."
    echo "  Use --install explicitly when you want to install packages."
    echo ""
    exit 0
fi

# =============================================================================
# INSTALLATION
# =============================================================================

install_dependencies() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Installing Dependencies${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    # Detect if running in CI
    if [ -n "$CI" ] || [ -n "$GITHUB_ACTIONS" ]; then
        echo -e "${YELLOW}Running in CI/CD environment${NC}"
        IS_CI=true
    else
        echo -e "${GREEN}Running in local environment${NC}"
        IS_CI=false
    fi

    # Check if running as root or with sudo
    if [ "$EUID" -eq 0 ]; then
        SUDO=""
        echo -e "${YELLOW}Running as root${NC}"
    else
        SUDO="sudo"
        echo -e "${GREEN}Using sudo for package installation${NC}"
    fi

    echo ""

    # Update package lists
    echo -e "${BLUE}Updating package lists...${NC}"
    $SUDO apt-get update -qq
    echo -e "${GREEN}✓ Package lists updated${NC}"
    echo ""

    # Install essential build tools
    echo -e "${BLUE}Installing essential build tools...${NC}"
    $SUDO apt-get install -y -qq \
        build-essential \
        cmake \
        ninja-build \
        git \
        pkg-config \
        curl \
        zip \
        unzip \
        tar \
        ca-certificates
    echo -e "${GREEN}✓ Essential build tools installed${NC}"
    echo ""

    # Install optional but recommended tools (for vcpkg packages)
    echo -e "${BLUE}Installing optional tools for vcpkg...${NC}"
    $SUDO apt-get install -y -qq \
        autoconf \
        automake \
        libtool \
        python3 \
        python3-pip
    echo -e "${GREEN}✓ Optional tools installed${NC}"
    echo ""

    # Check compiler versions
    echo -e "${BLUE}Verifying installed tools...${NC}"

    # GCC
    GCC_VERSION=$(gcc --version | head -n 1)
    echo -e "  GCC: ${GCC_VERSION}"
    GCC_MAJOR=$(gcc -dumpversion | cut -d. -f1)
    if [ "$GCC_MAJOR" -ge 10 ]; then
        echo -e "  ${GREEN}✓ GCC supports C++20${NC}"
    else
        echo -e "  ${RED}✗ GCC version too old for C++20 (need >= 10)${NC}"
        return 1
    fi

    # CMake
    CMAKE_VERSION=$(cmake --version | head -n 1 | grep -oP '\d+\.\d+\.\d+')
    echo -e "  CMake: ${CMAKE_VERSION}"
    CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
    CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)
    if [ "$CMAKE_MAJOR" -gt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -ge 16 ]); then
        echo -e "  ${GREEN}✓ CMake version sufficient (>= 3.16)${NC}"
    else
        echo -e "  ${RED}✗ CMake version too old (need >= 3.16)${NC}"
        return 1
    fi

    # Ninja
    NINJA_VERSION=$(ninja --version)
    echo -e "  Ninja: ${NINJA_VERSION}"
    echo -e "  ${GREEN}✓ Ninja found${NC}"

    # Git
    GIT_VERSION=$(git --version)
    echo -e "  Git: ${GIT_VERSION}"
    echo -e "  ${GREEN}✓ Git found${NC}"

    echo ""
    echo -e "${GREEN}✓ All tools verified${NC}"
    echo ""

    # Install/setup vcpkg
    echo -e "${BLUE}Setting up vcpkg...${NC}"

    # Determine vcpkg location
    if [ "$IS_CI" = true ]; then
        # In CI, use a specific location
        VCPKG_ROOT="${VCPKG_ROOT:-/opt/vcpkg}"
        echo -e "  CI mode: Installing vcpkg to ${VCPKG_ROOT}"
    else
        # Locally, use home directory
        VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
        echo -e "  Local mode: Installing vcpkg to ${VCPKG_ROOT}"
    fi

    # Clone vcpkg if it doesn't exist
    if [ ! -d "$VCPKG_ROOT" ]; then
        echo -e "  Cloning vcpkg repository..."
        $SUDO git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
        echo -e "  ${GREEN}✓ vcpkg cloned${NC}"
    else
        echo -e "  ${GREEN}✓ vcpkg directory exists${NC}"

        # Update vcpkg if in CI (always use latest in CI)
        if [ "$IS_CI" = true ]; then
            echo -e "  Updating vcpkg..."
            cd "$VCPKG_ROOT"
            $SUDO git pull --quiet
            echo -e "  ${GREEN}✓ vcpkg updated${NC}"
        fi
    fi

    # Bootstrap vcpkg if needed
    if [ ! -f "$VCPKG_ROOT/vcpkg" ]; then
        echo -e "  Bootstrapping vcpkg..."
        cd "$VCPKG_ROOT"
        $SUDO ./bootstrap-vcpkg.sh -disableMetrics
        echo -e "  ${GREEN}✓ vcpkg bootstrapped${NC}"
    else
        echo -e "  ${GREEN}✓ vcpkg already bootstrapped${NC}"
    fi

    # Set environment variable for current session
    export VCPKG_ROOT

    # Add to bashrc if not in CI and not already present
    if [ "$IS_CI" = false ]; then
        if ! grep -q "VCPKG_ROOT" "$HOME/.bashrc"; then
            echo "" >> "$HOME/.bashrc"
            echo "# vcpkg root (added by FrankyCPP setup script)" >> "$HOME/.bashrc"
            echo "export VCPKG_ROOT=\"$VCPKG_ROOT\"" >> "$HOME/.bashrc"
            echo -e "  ${GREEN}✓ VCPKG_ROOT added to ~/.bashrc${NC}"
        else
            echo -e "  ${GREEN}✓ VCPKG_ROOT already in ~/.bashrc${NC}"
        fi
    fi

    echo -e "${GREEN}✓ vcpkg setup complete${NC}"
    echo ""

    # Display vcpkg version
    VCPKG_VERSION=$($VCPKG_ROOT/vcpkg version | head -n 1)
    echo -e "  vcpkg version: ${VCPKG_VERSION}"
    echo ""
}

# =============================================================================
# VALIDATION
# =============================================================================

validate_environment() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Validating Build Environment${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    ERRORS=0
    WARNINGS=0

    # Function to check if a command exists
    check_command() {
        local cmd=$1
        local required=$2
        local package_hint=$3

        if command -v "$cmd" &> /dev/null; then
            local version=$($cmd --version 2>&1 | head -n 1)
            echo -e "${GREEN}✓${NC} $cmd found: $version"
            return 0
        else
            if [ "$required" = "required" ]; then
                echo -e "${RED}✗${NC} $cmd not found (REQUIRED)"
                if [ -n "$package_hint" ]; then
                    echo -e "  Install: ${YELLOW}$package_hint${NC}"
                fi
                ERRORS=$((ERRORS + 1))
            else
                echo -e "${YELLOW}!${NC} $cmd not found (optional)"
                if [ -n "$package_hint" ]; then
                    echo -e "  Install: ${YELLOW}$package_hint${NC}"
                fi
                WARNINGS=$((WARNINGS + 1))
            fi
            return 1
        fi
    }

    # Check compilers
    echo -e "${BLUE}Checking compilers...${NC}"
    check_command "gcc" "required" "sudo apt install build-essential"
    check_command "g++" "required" "sudo apt install build-essential"
    check_command "clang" "optional" "sudo apt install clang"

    # Check GCC version for C++20 support
    if command -v gcc &> /dev/null; then
        GCC_VERSION=$(gcc -dumpversion)
        GCC_MAJOR=$(echo $GCC_VERSION | cut -d. -f1)
        if [ "$GCC_MAJOR" -ge 10 ]; then
            echo -e "${GREEN}✓${NC} GCC $GCC_VERSION supports C++20"
        else
            echo -e "${RED}✗${NC} GCC $GCC_VERSION does not fully support C++20 (need >= 10.0)"
            ERRORS=$((ERRORS + 1))
        fi
    fi

    echo ""

    # Check build tools
    echo -e "${BLUE}Checking build tools...${NC}"
    check_command "cmake" "required" "sudo apt install cmake"
    check_command "ninja" "required" "sudo apt install ninja-build"
    check_command "make" "optional" "sudo apt install make"

    # Check CMake version
    if command -v cmake &> /dev/null; then
        CMAKE_VERSION=$(cmake --version | head -n 1 | grep -oP '\d+\.\d+\.\d+')
        CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
        CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)
        if [ "$CMAKE_MAJOR" -gt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -ge 16 ]); then
            echo -e "${GREEN}✓${NC} CMake $CMAKE_VERSION meets requirement (>= 3.16)"
        else
            echo -e "${RED}✗${NC} CMake $CMAKE_VERSION is too old (need >= 3.16)"
            ERRORS=$((ERRORS + 1))
        fi
    fi

    echo ""

    # Check utilities
    echo -e "${BLUE}Checking utilities...${NC}"
    check_command "git" "required" "sudo apt install git"
    check_command "pkg-config" "required" "sudo apt install pkg-config"
    check_command "curl" "required" "sudo apt install curl"
    check_command "tar" "required" "sudo apt install tar"
    check_command "unzip" "required" "sudo apt install unzip"
    check_command "zip" "optional" "sudo apt install zip"
    check_command "autoconf" "optional" "sudo apt install autoconf"
    check_command "automake" "optional" "sudo apt install automake"
    check_command "libtoolize" "optional" "sudo apt install libtool"

    echo ""

    # Check vcpkg
    echo -e "${BLUE}Checking vcpkg...${NC}"
    if [ -n "$VCPKG_ROOT" ]; then
        echo -e "${GREEN}✓${NC} VCPKG_ROOT is set: $VCPKG_ROOT"

        if [ -d "$VCPKG_ROOT" ]; then
            echo -e "${GREEN}✓${NC} VCPKG_ROOT directory exists"

            if [ -f "$VCPKG_ROOT/vcpkg" ]; then
                echo -e "${GREEN}✓${NC} vcpkg executable found"
                VCPKG_VERSION=$($VCPKG_ROOT/vcpkg version | head -n 1)
                echo -e "  Version: $VCPKG_VERSION"
            else
                echo -e "${RED}✗${NC} vcpkg executable not found in $VCPKG_ROOT"
                echo -e "  Run: cd $VCPKG_ROOT && ./bootstrap-vcpkg.sh"
                ERRORS=$((ERRORS + 1))
            fi
        else
            echo -e "${RED}✗${NC} VCPKG_ROOT directory does not exist: $VCPKG_ROOT"
            ERRORS=$((ERRORS + 1))
        fi
    else
        echo -e "${RED}✗${NC} VCPKG_ROOT environment variable not set"
        echo -e "  Run this script with --install to set up vcpkg"
        ERRORS=$((ERRORS + 1))
    fi

    echo ""

    # Check CPU features
    echo -e "${BLUE}Checking CPU features...${NC}"
    if [ -f /proc/cpuinfo ]; then
        if grep -q "avx2" /proc/cpuinfo; then
            echo -e "${GREEN}✓${NC} AVX2 supported"
        else
            echo -e "${YELLOW}!${NC} AVX2 not detected"
            WARNINGS=$((WARNINGS + 1))
        fi

        if grep -q "bmi2" /proc/cpuinfo; then
            echo -e "${GREEN}✓${NC} BMI2 (PEXT) supported"
        else
            echo -e "${YELLOW}!${NC} BMI2 not detected - will need to disable PEXT optimizations"
            WARNINGS=$((WARNINGS + 1))
        fi

        if grep -q "popcnt" /proc/cpuinfo; then
            echo -e "${GREEN}✓${NC} POPCNT supported"
        else
            echo -e "${YELLOW}!${NC} POPCNT not detected"
            WARNINGS=$((WARNINGS + 1))
        fi
    else
        echo -e "${YELLOW}!${NC} Cannot detect CPU features (/proc/cpuinfo not found)"
        WARNINGS=$((WARNINGS + 1))
    fi

    echo ""

    # Check project structure
    echo -e "${BLUE}Checking project structure...${NC}"
    if [ -f "CMakeLists.txt" ]; then
        echo -e "${GREEN}✓${NC} CMakeLists.txt found"
    else
        echo -e "${RED}✗${NC} CMakeLists.txt not found (run from project root)"
        ERRORS=$((ERRORS + 1))
    fi

    if [ -f "vcpkg.json" ]; then
        echo -e "${GREEN}✓${NC} vcpkg.json found (manifest mode)"
    else
        echo -e "${RED}✗${NC} vcpkg.json not found"
        ERRORS=$((ERRORS + 1))
    fi

    if [ -f "build_wsl.sh" ]; then
        echo -e "${GREEN}✓${NC} build_wsl.sh found"
        if [ -x "build_wsl.sh" ]; then
            echo -e "${GREEN}✓${NC} build_wsl.sh is executable"
        else
            echo -e "${YELLOW}!${NC} build_wsl.sh is not executable (run: chmod +x build_wsl.sh)"
            WARNINGS=$((WARNINGS + 1))
        fi
    else
        echo -e "${YELLOW}!${NC} build_wsl.sh not found"
        WARNINGS=$((WARNINGS + 1))
    fi

    echo ""

    # Summary
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Validation Summary${NC}"
    echo -e "${BLUE}========================================${NC}"

    if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
        echo -e "${GREEN}✓ All checks passed!${NC}"
        echo -e "${GREEN}Your build environment is ready.${NC}"
        return 0
    elif [ $ERRORS -eq 0 ]; then
        echo -e "${YELLOW}! $WARNINGS warning(s) found${NC}"
        echo -e "${GREEN}Build should work, but some features may be unavailable.${NC}"
        return 0
    else
        echo -e "${RED}✗ $ERRORS error(s) found${NC}"
        if [ $WARNINGS -gt 0 ]; then
            echo -e "${YELLOW}! $WARNINGS warning(s) found${NC}"
        fi
        echo -e "${RED}Please fix the errors above before building.${NC}"
        return 1
    fi
}

# =============================================================================
# MAIN
# =============================================================================

main() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}FrankyCPP Linux Build Environment Setup${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    case "$MODE" in
        install)
            echo -e "${YELLOW}INSTALLING dependencies (requires sudo)${NC}"
            echo ""
            install_dependencies
            echo ""
            validate_environment
            ;;
        validate)
            echo -e "${GREEN}VALIDATING environment (safe, no modifications)${NC}"
            echo ""
            validate_environment
            ;;
    esac

    local exit_code=$?

    echo ""
    echo -e "${BLUE}========================================${NC}"

    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}Setup complete!${NC}"

        if [ "$MODE" = "install" ]; then
            if [ -n "$CI" ] || [ -n "$GITHUB_ACTIONS" ]; then
                echo -e "${GREEN}Ready for CI/CD build${NC}"
            else
                echo -e "${YELLOW}Next steps:${NC}"
                echo -e "  1. Run: ${GREEN}source ~/.bashrc${NC} (to load VCPKG_ROOT)"
                echo -e "  2. Configure: ${GREEN}cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release${NC}"
                echo -e "  3. Build: ${GREEN}cmake --build build${NC}"
            fi
        elif [ "$MODE" = "validate" ]; then
            # Check if there were errors during validation
            if grep -q "error(s) found" <<< "$validation_output" 2>/dev/null || [ $exit_code -ne 0 ]; then
                echo -e "${YELLOW}To install missing dependencies, run:${NC}"
                echo -e "  ${GREEN}$0 --install${NC}"
            fi
        fi
    else
        echo -e "${RED}Setup encountered errors${NC}"
        if [ "$MODE" = "validate" ]; then
            echo -e "${YELLOW}To install missing dependencies, run:${NC}"
            echo -e "  ${GREEN}$0 --install${NC}"
        fi
    fi

    echo -e "${BLUE}========================================${NC}"

    return $exit_code
}

# Run main function
main
exit $?
