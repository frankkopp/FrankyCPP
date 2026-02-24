message(STATUS "====================================================================")
message(STATUS "FrankyCPP Main")
message(STATUS "CMAKE ${CMAKE_VERSION} ${CMAKE_COMMAND}")
message(STATUS "====================================================================")

cmake_minimum_required(VERSION 3.16)

# Project name and version
project(FrankyCPP VERSION 1.4.0)
message(STATUS "FrankyCPP v${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

# Enforce C++20 globally so all targets (including FetchContent deps) see the right standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build all third-party libs statically unless a subproject overrides it
set(BUILD_SHARED_LIBS OFF)

# CI-friendly toggle: treat warnings as errors when enabled
option(STRICT_WARNINGS "Treat all warnings as errors" OFF)
# Debug-only sanitizers (ASan/UBSan where supported)
option(ENABLE_SANITIZERS "Enable Address/Undefined sanitizers in Debug builds" OFF)

# updates template file and replaces placeholder variables
configure_file(src/version.h.in version.h)

# Prefer portable standard mode over gnu++ extensions
set(CMAKE_CXX_EXTENSIONS OFF)

# CI-friendly toggles
option(FRANKYCPP_BUILD_TESTS "Build unit tests" ON)
option(FRANKYCPP_BUILD_BENCHMARKS "Build benchmarks" ON)
option(FRANKYCPP_USE_PCH "Enable precompiled headers" ON)
option(ENABLE_UNITY_BUILD "Enable Unity/Jumbo builds" OFF)
option(FRANKYCPP_PRODUCTION "Production build: freeze non-essential configs as static constexpr" OFF)

if(ENABLE_SANITIZERS)
    if(MSVC)
        message(STATUS "Sanitizers enabled (MSVC): AddressSanitizer in Debug")
        add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
        add_link_options($<$<CONFIG:Debug>:/fsanitize=address>)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(STATUS "Sanitizers enabled: AddressSanitizer/UndefinedBehaviorSanitizer in Debug")
        add_compile_options($<$<CONFIG:Debug>:-fsanitize=address,undefined -fno-omit-frame-pointer>)
        add_link_options($<$<CONFIG:Debug>:-fsanitize=address,undefined>)
    else()
        message(WARNING "ENABLE_SANITIZERS is ON but compiler is not recognized for sanitizer support")
    endif()
endif()

# Ensure no compiler launcher is set from a previous cache
unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)

# Unit testing enabled only if requested
if (FRANKYCPP_BUILD_TESTS)
    enable_testing()
endif()

# Disable Boost auto-linking; we link explicitly via CMake
add_compile_definitions(BOOST_ALL_NO_LIB)

# Platform
message(STATUS "Recognized platform is ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}")

# Compiler Settings
message(STATUS "Compiler is ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} ${CMAKE_CXX_COMPILER}")

# =============================================================================
# Build Tool Validation (for local builds and CI/CD)
# =============================================================================
message(STATUS "")
message(STATUS "Validating build environment...")

# Check C++20 compiler support
if(CMAKE_CXX_STANDARD LESS 20)
    message(FATAL_ERROR "C++20 or higher is required. Current: C++${CMAKE_CXX_STANDARD}")
endif()

# Validate compiler version requirements
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13.0")
        message(FATAL_ERROR "GCC 13.0 or higher is required for full C++20 support (std::format). Found: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    message(STATUS "✓ GCC ${CMAKE_CXX_COMPILER_VERSION} supports full C++20 (including std::format)")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # On Linux, always use libstdc++ for compatibility with vcpkg libraries
    # vcpkg builds libraries with GCC's libstdc++, so we need to match that
    if(UNIX AND NOT APPLE)
        add_compile_options(-stdlib=libstdc++)
        add_link_options(-stdlib=libstdc++)

        # Verify GCC 13+ is available (provides libstdc++ with std::format)
        find_program(GXX_PATH g++-13 g++)
        if(GXX_PATH)
            execute_process(
                COMMAND ${GXX_PATH} -dumpversion
                OUTPUT_VARIABLE GCC_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(GCC_VERSION VERSION_LESS "13.0")
                message(FATAL_ERROR "GCC 13+ required for libstdc++ with std::format. Found GCC ${GCC_VERSION}")
            else()
                message(STATUS "✓ Clang ${CMAKE_CXX_COMPILER_VERSION} with libstdc++ from GCC ${GCC_VERSION} (std::format available)")
            endif()
        else()
            message(FATAL_ERROR "GCC 13+ not found. Install with: sudo apt install g++-13")
        endif()
    else()
        message(STATUS "✓ Clang ${CMAKE_CXX_COMPILER_VERSION} configured for C++20")
    endif()
elseif(MSVC)
    if(MSVC_VERSION LESS 1929)
        message(FATAL_ERROR "MSVC 2019 16.10 or higher is required for C++20 support. Found: ${MSVC_VERSION}")
    endif()
    message(STATUS "✓ MSVC ${CMAKE_CXX_COMPILER_VERSION} supports C++20")
endif()

# Platform-specific build tool checks
if(WIN32)
    # Windows-specific checks
    # Check if we're in a proper MSVC environment (if using MSVC)
    if(MSVC)
        # Check for vcvarsall.bat environment (only meaningful in fresh shell)
        if(NOT DEFINED ENV{VSINSTALLDIR} AND NOT DEFINED ENV{DevEnvDir})
            message(WARNING "MSVC environment may not be initialized.")
            message(WARNING "If building outside an IDE, run vcvarsall.bat or use 'x64 Native Tools Command Prompt'")
        else()
            message(STATUS "✓ MSVC environment detected")
        endif()
    endif()

    # Ninja is highly recommended on Windows for faster builds
    if(CMAKE_GENERATOR MATCHES "Ninja")
        message(STATUS "✓ Using Ninja generator (recommended)")
    elseif(CMAKE_GENERATOR MATCHES "Visual Studio")
        message(STATUS "Using Visual Studio generator")
    else()
        message(WARNING "Consider using Ninja generator for faster builds: -G Ninja")
    endif()

elseif(UNIX)
    # Unix/Linux-specific checks
    # Check for pkg-config (needed by vcpkg for some packages)
    find_program(PKG_CONFIG_EXECUTABLE pkg-config)
    if(NOT PKG_CONFIG_EXECUTABLE)
        message(WARNING "pkg-config not found. Some vcpkg packages may fail to install.")
        message(WARNING "Install with: sudo apt install pkg-config  (Ubuntu/Debian)")
    else()
        message(STATUS "✓ pkg-config found: ${PKG_CONFIG_EXECUTABLE}")
    endif()

    # Check for essential build tools
    find_program(MAKE_EXECUTABLE make)
    find_program(TAR_EXECUTABLE tar)
    find_program(UNZIP_EXECUTABLE unzip)

    if(NOT TAR_EXECUTABLE)
        message(WARNING "tar not found. vcpkg may fail to extract archives.")
    endif()
    if(NOT UNZIP_EXECUTABLE)
        message(WARNING "unzip not found. vcpkg may fail to extract archives.")
    endif()
endif()

# Validate vcpkg setup
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    message(STATUS "✓ vcpkg toolchain detected: ${CMAKE_TOOLCHAIN_FILE}")

    # Extract and validate VCPKG_ROOT
    get_filename_component(VCPKG_ROOT_DERIVED "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
    get_filename_component(VCPKG_ROOT_DERIVED "${VCPKG_ROOT_DERIVED}" DIRECTORY)
    get_filename_component(VCPKG_ROOT_DERIVED "${VCPKG_ROOT_DERIVED}" DIRECTORY)

    # Check for vcpkg executable (platform-specific)
    set(VCPKG_FOUND FALSE)
    if(WIN32)
        if(EXISTS "${VCPKG_ROOT_DERIVED}/vcpkg.exe")
            message(STATUS "✓ vcpkg.exe found")
            set(VCPKG_FOUND TRUE)
        endif()
    else()
        if(EXISTS "${VCPKG_ROOT_DERIVED}/vcpkg")
            message(STATUS "✓ vcpkg executable found")
            set(VCPKG_FOUND TRUE)
        endif()
    endif()

    if(NOT VCPKG_FOUND)
        message(WARNING "vcpkg executable not found in ${VCPKG_ROOT_DERIVED}")
        if(WIN32)
            message(WARNING "Run bootstrap: ${VCPKG_ROOT_DERIVED}\\bootstrap-vcpkg.bat")
        else()
            message(WARNING "Run bootstrap: ${VCPKG_ROOT_DERIVED}/bootstrap-vcpkg.sh")
        endif()
    endif()

    # Check for vcpkg.json manifest
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json")
        message(STATUS "✓ vcpkg manifest mode (vcpkg.json found)")
    else()
        message(WARNING "vcpkg.json not found - classic mode or missing manifest")
    endif()
else()
    message(WARNING "vcpkg toolchain not detected. Dependencies may not be found.")
    message(WARNING "Set CMAKE_TOOLCHAIN_FILE to point to vcpkg.cmake")
    if(WIN32)
        message(WARNING "Example: -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake")
    else()
        message(WARNING "Example: -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake")
        message(WARNING "Or set via environment: export VCPKG_ROOT=~/vcpkg")
    endif()
endif()

# Check CPU features for optimization flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|amd64)")
    message(STATUS "✓ x86_64 architecture detected")
    # Note: Actual CPUID checks would require try_compile or external tools
    # We'll rely on -march=native on GCC/Clang, which will fail at compile time if unsupported
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "(aarch64|ARM64)")
    message(STATUS "✓ ARM64 architecture detected")
else()
    message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

message(STATUS "Build environment validation complete")
message(STATUS "")
# =============================================================================

# On MSVC choose the static vcpkg triplet by default so third-party libs (e.g., Boost)
# are linked statically while keeping the MSVC CRT dynamic (/MD).
# Users can still override by passing -DVCPKG_TARGET_TRIPLET.
if (MSVC)
    if (NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "vcpkg triplet" FORCE)
        message(STATUS "VCPKG_TARGET_TRIPLET not set by user; defaulting to ${VCPKG_TARGET_TRIPLET}")
    else()
        message(STATUS "VCPKG_TARGET_TRIPLET is ${VCPKG_TARGET_TRIPLET}")
    endif()
endif()

# Derive the vcpkg installed dir in this build tree when using manifest mode.
# CLion/manifest mode typically places it under <build>/vcpkg_installed.
if (NOT DEFINED VCPKG_INSTALLED_DIR)
    set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed" CACHE PATH "vcpkg installed dir")
endif()
message(STATUS "VCPKG_INSTALLED_DIR is ${VCPKG_INSTALLED_DIR}")

# Guard: avoid building with the dynamic vcpkg triplet by accident
if (WIN32)
    if (DEFINED VCPKG_TARGET_TRIPLET)
        string(FIND "${VCPKG_TARGET_TRIPLET}" "-windows" _dyn_pos)
        if (_dyn_pos GREATER -1 AND NOT VCPKG_TARGET_TRIPLET MATCHES "-static")
            message(FATAL_ERROR "VCPKG_TARGET_TRIPLET='${VCPKG_TARGET_TRIPLET}' is a dynamic triplet. Use 'x64-windows-static-md' (or -static) to produce a DLL-free executable.")
        endif()
        message(STATUS "Using VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")
    endif()
endif()

# Microsoft Visual Compiler configuration (Windows/MSVC)
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

    # this needs to be added to CLION cmake options: -G Ninja
    message(STATUS "Compiler Settings: MSVC")

    # Use Embedded debug info to avoid PDB contention with parallel builds
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>" CACHE STRING "MSVC debug info format" FORCE)
    message(STATUS "MSVC debug info: ${CMAKE_MSVC_DEBUG_INFORMATION_FORMAT}")

    # Prefer IPO over manual flags; enable only if supported
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_msg)
    if(_ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
        message(STATUS "IPO/LTO enabled for Release/RelWithDebInfo/MinSizeRel")
    else()
        message(STATUS "IPO/LTO not supported: ${_ipo_msg}")
    endif()

    # Ensure linker is compatible with LTCG in Release
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO")

    # Ensure we use dynamic CRT (/MD in Release, /MDd in Debug) consistently
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    # Base and per-config flags (no manual /GL, /arch gated via option)
    set(CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /favor:INTEL64 /W4 /GR /EHsc /MP /FS")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob3 /Oy /Oi /Ot /GT /Gw /Gy /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Ob0 /RTC1 /permissive- /Zi")

    # Feature toggles
    option(ENABLE_STD_EXECUTION "Define HAS_EXECUTION_LIB to enable <execution> usage" ON)
    if(ENABLE_STD_EXECUTION)
        message(STATUS "HAS_EXECUTION_LIB enabled")
    endif()

    option(ENABLE_BMI2_PEXT "Define HAS_PEXT and enable BMI2-related code paths" ON)
    if(ENABLE_BMI2_PEXT)
        message(STATUS "HAS_PEXT enabled; adding /arch:AVX2 for BMI2 support")
    endif()

    option(ENABLE_AVX2 "Enable AVX2 instruction set (/arch:AVX2)" OFF)
    if(ENABLE_AVX2)
        message(STATUS "AVX2 enabled (/arch:AVX2)")
    endif()

# =============================================================================
# GCC/Clang Compiler Configuration (Linux/WSL)
# =============================================================================
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Compiler Settings: ${CMAKE_CXX_COMPILER_ID}")

    # Prefer IPO over manual flags; enable only if supported
    # Note: Disable LTO for Clang due to linking issues with static libraries
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_msg)
    if(_ipo_supported AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
        message(STATUS "IPO/LTO enabled for Release/RelWithDebInfo/MinSizeRel")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "IPO/LTO disabled for Clang (linking issues with static libraries)")
    else()
        message(STATUS "IPO/LTO not supported: ${_ipo_msg}")
    endif()

    # Base flags: C++20, warnings, CPU features
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -march=native -mpopcnt -mbmi2")

    # Per-config optimization flags
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

    # Feature toggles
    option(ENABLE_STD_EXECUTION "Define HAS_EXECUTION_LIB to enable <execution> usage" OFF)
    if(ENABLE_STD_EXECUTION)
        message(STATUS "HAS_EXECUTION_LIB enabled")
        add_compile_definitions(HAS_EXECUTION_LIB)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            # GCC requires explicit TBB linkage for parallel algorithms
            find_package(TBB REQUIRED)
            link_libraries(TBB::tbb)
        endif()
    endif()

    option(ENABLE_BMI2_PEXT "Define HAS_PEXT and enable BMI2-related code paths" ON)
    if(ENABLE_BMI2_PEXT)
        message(STATUS "HAS_PEXT enabled (via -march=native -mbmi2)")
        add_compile_definitions(HAS_PEXT)
    endif()

else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif ()

# Print some info to be able to check which compiler has been picked
message(STATUS "Generator: ${CMAKE_GENERATOR} ${CMAKE_MAKE_PROGRAM}")
message(STATUS "Compiler flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Compiler DEBUG flags: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "Compiler RELEASE flags: ${CMAKE_CXX_FLAGS_RELEASE}")

# Boost via vcpkg CONFIG package (kept)
find_package(Boost CONFIG REQUIRED COMPONENTS program_options serialization filesystem)

# Use spdlog from vcpkg instead of FetchContent
find_package(spdlog CONFIG REQUIRED)

# yaml-cpp via vcpkg CONFIG package
find_package(yaml-cpp CONFIG REQUIRED)

# nlohmann_json for JSON parsing in Arena (header-only)
find_package(nlohmann_json CONFIG REQUIRED)

# OpenSSL for MD5 checksums in tablebase verification
find_package(OpenSSL REQUIRED)

# CURL for HTTP downloads (tablebase download)
find_package(CURL REQUIRED)

# =============================================================================
# Fathom Tablebase Library (Syzygy probing)
# =============================================================================
include(cmake/Fathom.cmake)

# project sub directories - need to have their own CMakeList.txt
add_subdirectory(src)

if (FRANKYCPP_BUILD_TESTS)
    add_subdirectory(test)
endif()

if (FRANKYCPP_BUILD_BENCHMARKS)
    add_subdirectory(testbench)
endif()


# =============================================================================
# Configuration Summary (useful for CI/CD logs)
# =============================================================================
message(STATUS "")
message(STATUS "====================================================================")
message(STATUS "FrankyCPP Build Configuration Summary")
message(STATUS "====================================================================")
message(STATUS "Version:            ${PROJECT_VERSION}")
message(STATUS "Build Type:         ${CMAKE_BUILD_TYPE}")
message(STATUS "Platform:           ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}")
message(STATUS "Architecture:       ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler:           ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard:       C++${CMAKE_CXX_STANDARD}")
message(STATUS "Generator:          ${CMAKE_GENERATOR}")
if(CMAKE_MAKE_PROGRAM)
    message(STATUS "Build Tool:         ${CMAKE_MAKE_PROGRAM}")
endif()
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Build Tests:      ${FRANKYCPP_BUILD_TESTS}")
message(STATUS "  Build Benchmarks: ${FRANKYCPP_BUILD_BENCHMARKS}")
message(STATUS "  Use PCH:          ${FRANKYCPP_USE_PCH}")
message(STATUS "  Unity Build:      ${ENABLE_UNITY_BUILD}")
message(STATUS "  Strict Warnings:  ${STRICT_WARNINGS}")
message(STATUS "  Sanitizers:       ${ENABLE_SANITIZERS}")
message(STATUS "  Production Mode:  ${FRANKYCPP_PRODUCTION}")
if(DEFINED ENABLE_STD_EXECUTION)
    message(STATUS "  std::execution:   ${ENABLE_STD_EXECUTION}")
endif()
if(DEFINED ENABLE_BMI2_PEXT)
    message(STATUS "  BMI2/PEXT:        ${ENABLE_BMI2_PEXT}")
endif()
if(DEFINED ENABLE_AVX2)
    message(STATUS "  AVX2:             ${ENABLE_AVX2}")
endif()
message(STATUS "")
if(DEFINED VCPKG_TARGET_TRIPLET)
    message(STATUS "vcpkg Triplet:      ${VCPKG_TARGET_TRIPLET}")
endif()
if(DEFINED VCPKG_INSTALLED_DIR)
    message(STATUS "vcpkg Install Dir:  ${VCPKG_INSTALLED_DIR}")
endif()
message(STATUS "====================================================================")
message(STATUS "")
# =============================================================================
