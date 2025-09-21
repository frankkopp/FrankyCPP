message(STATUS "====================================================================")
message(STATUS "FrankyCPP Main")
message(STATUS "CMAKE ${CMAKE_VERSION} ${CMAKE_COMMAND}")
message(STATUS "Compiler is ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} ${CMAKE_CXX_COMPILER}")
message(STATUS "====================================================================")

cmake_minimum_required(VERSION 3.16)

# Project name and version
project(FrankyCPP VERSION 0.5)
message(STATUS "FrankyCPP v${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

# Build all third-party libs statically unless a subproject overrides it
set(BUILD_SHARED_LIBS OFF)

# CI-friendly toggle: treat warnings as errors when enabled
option(STRICT_WARNINGS "Treat all warnings as errors" OFF)

# updates template file and replaces placeholder variables
configure_file(src/version.h.in version.h)

# Prefer portable standard mode over gnu++ extensions
set(CMAKE_CXX_EXTENSIONS OFF)

# CI-friendly toggles
option(FRANKYCPP_BUILD_TESTS "Build unit tests" ON)
option(FRANKYCPP_BUILD_BENCHMARKS "Build benchmarks" ON)
option(FRANKYCPP_USE_PCH "Enable precompiled headers" ON)
option(ENABLE_UNITY_BUILD "Enable Unity/Jumbo builds" OFF)

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

# Microsoft Visual Compiler only (non-MSVC toolchains are commented out below for later work)
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

    # this needs to be added to CLION cmake options: -G Ninja
    message(STATUS "Compiler Settings: MSVC")

    # Prefer IPO over manual /GL and /LTCG flags
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)

    # Ensure linker is compatible with LTCG in Release
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO")

    # Ensure we use dynamic CRT (/MD in Release, /MDd in Debug) consistently
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    # Base and per-config flags (no manual /GL, /arch gated via option)
    set(CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /favor:INTEL64 /W4 /GR /EHsc /MP /Oi /Ot")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob3 /Oy /Oi /Ot /GT /Gw /Gy /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Ob0 /Zi /RTC1 /permissive-")

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

endif ()

# -----------------------------------------------------------------------------
# Non-MSVC toolchains are intentionally commented out for now and will be
# revisited later. Keeping them here for reference.
# -----------------------------------------------------------------------------
#  # GNU compiler for Windows WSL and Travis
# elseif (CMAKE_COMPILER_IS_GNUCXX)
#     message(STATUS "Compiler Settings: GNU")
#
#     set(CMAKE_CXX_FLAGS "-std=c++2a -m64 -msse -msse3 -msse4 -mpopcnt -mbmi2 -Wall -Wextra -Wpedantic -Wno-error=pedantic -Wno-unknown-pragmas -Wno-variadic-macros -Wno-sign-compare -Wno-subobject-linkage")
#     set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 --coverage -Wno-unused-parameter -Wno-unused-variable")
#     set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
#     # additional: -Wshadow  -Wno-unknown-pragmas -Weffc++ -Wimplicit-fallthrough -Wswitch -Wpointer-arith -Wcast-qual -Wconversion -Wno-sign-conversion -Wno-error=padded -Wno-error=inline")
#
#     # link time optimization on Travis does not work (yet)
#     # # Test for -flto support
#     # include(CheckCXXCompilerFlag)
#     # include(CheckCCompilerFlag)
#     # CHECK_CXX_COMPILER_FLAG(-flto CXX_SUPPORTS_FLTO)
#     # CHECK_C_COMPILER_FLAG(-flto C_SUPPORTS_FLTO)
#     # if(CXX_SUPPORTS_FLTO AND C_SUPPORTS_FLTO)
#     #     message(STATUS "Compiling with FLTO support")
#     #     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
#     #     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
#     #     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
#     #     set(CMAKE_AR "gcc-ar")
#     #     set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcs <LINK_FLAGS> ")
#     #     set(CMAKE_C_ARCHIVE_FINISH true)
#     #     set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcs <LINK_FLAGS> ")
#     #     set(CMAKE_CXX_ARCHIVE_FINISH true)
#     # endif()
#
#     # Test if we are on a Travis environment
#     if(DEFINED ENV{TRAVIS})
#         message(STATUS "TRAVIS BUILD")
#     else()
#         message(STATUS "NONE TRAVIS BUILD")
#         set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
#         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
#     endif()
#
#     # Availability of PEXT compiler intrinsic. Not available on all platforms
#     message(STATUS "Using HAS_PEXT")
#     add_compile_definitions(HAS_PEXT)
#
#     # std::execution lib for parallel std operations. Only on gcc-10 and above
#     if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "10")
#         message(STATUS "GCC >= 10.x")
#         message(STATUS "Using HAS_EXECUTION_LIB")
# #        add_compile_definitions(HAS_EXECUTION_LIB)
#     else ()
#         message(STATUS "GCC < 10.x")
#     endif ()
#
# # Clang compiler - needs more testing
# elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#     message(STATUS "Compiler Settings: Clang")
#     set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -Wextra -mpopcnt -mbmi2")
#     set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fprofile-instr-generate -fcoverage-mapping -Wpedantic -Wshadow -Wno-conversion -Wunreachable-code -Wuninitialized  -Wold-style-cast -Wunused-variable -Wfloat-equal -Wno-gnu-zero-variadic-macro-arguments")
#     set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
#
#     # std::execution lib for parallel std operations. Not available on all platforms
#     message(STATUS "Using HAS_EXECUTION_LIB")
#     add_compile_definitions(HAS_EXECUTION_LIB)
#     # Availability of PEXT compiler intrinsic. Not available on all platforms
#     message(STATUS "Using HAS_PEXT")
#     add_compile_definitions(HAS_PEXT)
#
# # Apple OS X compiler
# elseif (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
#     message(STATUS "Compiler Settings: AppleClang")
#     set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -Wextra -mpopcnt -mbmi2")
#     set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fprofile-instr-generate -fcoverage-mapping -Wpedantic -Wshadow -Wno-conversion -Wunreachable-code -Wuninitialized  -Wold-style-cast -Wunused-variable -Wfloat-equal -Wno-gnu-zero-variadic-macro-arguments")
#     set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
#
#     # std::execution lib for parallel std operations. Not available on all platforms
#     # message(STATUS "Using HAS_EXECUTION_LIB")
#     # add_compile_definitions(HAS_EXECUTION_LIB)
#     # Availability of PEXT compiler intrinsic. Not available on all platforms
#     message(STATUS "Using HAS_PEXT")
#     add_compile_definitions(HAS_PEXT)
# endif ()

# Print some info to be able to check which compiler has been picked
message(STATUS "Generator: ${CMAKE_GENERATOR} ${CMAKE_MAKE_PROGRAM}")
message(STATUS "Compiler flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Compiler DEBUG flags: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "Compiler RELEASE flags: ${CMAKE_CXX_FLAGS_RELEASE}")

# Boost via vcpkg CONFIG package (kept)
set(Boost_NO_WARN_NEW_VERSIONS ON)
find_package(Boost CONFIG REQUIRED COMPONENTS program_options serialization)

# For downloading with GIT (required by FetchContent)
message(STATUS "Finding GIT...")
set(GIT_EXECUTABLE)
find_package(Git)
if (GIT_FOUND)
    message(STATUS "GIT found: ${GIT_EXECUTABLE}")
else()
    message(FATAL_ERROR "GIT NOT found:")
endif ()

set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
include(FetchContent)

# FMT pinned
message(STATUS "Downloading/Update FMT (pinned)")
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 8.0.1
)
FetchContent_MakeAvailable(fmt)

# SPDLOG pinned (using external fmt)
message(STATUS "Downloading/Update SPDLOG (pinned)")
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.10.0
)
option(SPDLOG_FMT_EXTERNAL "Use external fmt" ON)
set(SPDLOG_FMT_EXTERNAL ON)
FetchContent_MakeAvailable(spdlog)

# GOOGLE TEST pinned (only if building tests)
if (FRANKYCPP_BUILD_TESTS)
    message(STATUS "Downloading/Update Google Test (pinned)")
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.13.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    option(INSTALL_GMOCK "Install GMock" OFF)
    option(INSTALL_GTEST "Install GTest" OFF)
    FetchContent_MakeAvailable(googletest)
endif()

# GOOGLE BENCHMARK pinned (only if building benchmarks)
if (FRANKYCPP_BUILD_BENCHMARKS)
    message(STATUS "Downloading/Update Google Benchmark (pinned)")
    FetchContent_Declare(
            googlebenchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.7.1
    )
    option(BENCHMARK_ENABLE_LTO "Enable LTO" ON)
    option(BENCHMARK_ENABLE_GTEST_TESTS "Build benchmark unit tests" OFF)
    option(BENCHMARK_ENABLE_TESTING "Enable benchmark tests" OFF)
    option(BENCHMARK_ENABLE_INSTALL "Install benchmark" OFF)
    FetchContent_MakeAvailable(googlebenchmark)
endif()

# include sub directories
# Removed global include directories for src/test/testbench; handled per-target instead
# include_directories(
#         ${PROJECT_SOURCE_DIR}/src
#         ${PROJECT_SOURCE_DIR}/test
#         ${PROJECT_SOURCE_DIR}/testbench
# )

# project sub directories - need to have their own CMakeList.txt
add_subdirectory(src)

if (FRANKYCPP_BUILD_TESTS)
    add_subdirectory(test)
endif()

if (FRANKYCPP_BUILD_BENCHMARKS)
    add_subdirectory(testbench)
endif()
