message("====================================================================")
message("FrankyCPP Main")
message("CMAKE " ${CMAKE_VERSION} " " ${CMAKE_COMMAND})
message("Compiler is " ${CMAKE_CXX_COMPILER_ID} " " ${CMAKE_CXX_COMPILER_VERSION} " " ${CMAKE_CXX_COMPILER})
message("====================================================================")

cmake_minimum_required(VERSION 3.16)

# Project name and version
project(FrankyCPP VERSION 0.5)
message("FrankyCPP v" ${PROJECT_VERSION_MAJOR} "." ${PROJECT_VERSION_MINOR})

# Build all third-party libs statically unless a subproject overrides it
set(BUILD_SHARED_LIBS OFF)

# updates template file and replaces placeholder variables
configure_file(src/version.h.in version.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# C++ settings
set(CMAKE_CXX_STANDARD 20)

# Unit testing enabled
enable_testing()

# Disable Boost auto-linking; we link explicitly via CMake
add_compile_definitions(BOOST_ALL_NO_LIB)

# Platform
message("Recognized platform is " ${CMAKE_SYSTEM_NAME} " " ${CMAKE_SYSTEM_VERSION})

# Compiler Settings
message("Compiler is " ${CMAKE_CXX_COMPILER_ID} " " ${CMAKE_CXX_COMPILER_VERSION} " " ${CMAKE_CXX_COMPILER})

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

# Microsoft Visual Compiler
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
# this needs to be added to CLION cmake options
# -G Ninja
# /LTCG  throws errors
message("Compiler Settings: MSVC")
set(CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /favor:INTEL64 /Gr /W4 /w14640 /GR /EHsc /arch:AVX2 /MP /Oi /Ot /GL")
set(CMAKE_CXX_FLAGS_RELEASE "/Qpar /O2 /Ob3 /Oy /Oi /Ot /GT /Gw /Gy /GL /MD /DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "/Od /Ob0 /MDd /Zi /RTC1 /permissive-")

# std::execution lib for parallel std operations. Not available on all platforms
message("Using HAS_EXECUTION_LIB")
add_compile_definitions(HAS_EXECUTION_LIB)
# Availability of PEXT compiler intrinsic. Not available on all platforms
message("Using HAS_PEXT")
add_compile_definitions(HAS_PEXT)

 # GNU compiler for Windows WSL and Travis
elseif (CMAKE_COMPILER_IS_GNUCXX)
    message("Compiler Settings: GNU")

    set(CMAKE_CXX_FLAGS "-std=c++2a -m64 -msse -msse3 -msse4 -mpopcnt -mbmi2 -Wall -Wextra -Wpedantic -Wno-error=pedantic -Wno-unknown-pragmas -Wno-variadic-macros -Wno-sign-compare -Wno-subobject-linkage")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 --coverage -Wno-unused-parameter -Wno-unused-variable")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    # additional: -Wshadow  -Wno-unknown-pragmas -Weffc++ -Wimplicit-fallthrough -Wswitch -Wpointer-arith -Wcast-qual -Wconversion -Wno-sign-conversion -Wno-error=padded -Wno-error=inline")

    # link time optimization on Travis does not work (yet)
    # # Test for -flto support
    # include(CheckCXXCompilerFlag)
    # include(CheckCCompilerFlag)
    # CHECK_CXX_COMPILER_FLAG(-flto CXX_SUPPORTS_FLTO)
    # CHECK_C_COMPILER_FLAG(-flto C_SUPPORTS_FLTO)
    # if(CXX_SUPPORTS_FLTO AND C_SUPPORTS_FLTO)
    #     message(STATUS "Compiling with FLTO support")
    #     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
    #     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    #     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
    #     set(CMAKE_AR "gcc-ar")
    #     set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcs <LINK_FLAGS> ")
    #     set(CMAKE_C_ARCHIVE_FINISH true)
    #     set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcs <LINK_FLAGS> ")
    #     set(CMAKE_CXX_ARCHIVE_FINISH true)
    # endif()

    # Test if we are on a Travis environment
    if(DEFINED ENV{TRAVIS})
        message("TRAVIS BUILD")
    else()
        message("NONE TRAVIS BUILD")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    endif()

    # Availability of PEXT compiler intrinsic. Not available on all platforms
    message("Using HAS_PEXT")
    add_compile_definitions(HAS_PEXT)

    # std::execution lib for parallel std operations. Only on gcc-10 and above
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "10")
        message("GCC >= 10.x")
        message("Using HAS_EXECUTION_LIB")
#        add_compile_definitions(HAS_EXECUTION_LIB)
    else ()
        message("GCC < 10.x")
    endif ()

# Clang compiler - needs more testing
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message("Compiler Settings: Clang")
    set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -Wextra -mpopcnt -mbmi2")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fprofile-instr-generate -fcoverage-mapping -Wpedantic -Wshadow -Wno-conversion -Wunreachable-code -Wuninitialized  -Wold-style-cast -Wunused-variable -Wfloat-equal -Wno-gnu-zero-variadic-macro-arguments")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

    # std::execution lib for parallel std operations. Not available on all platforms
    message("Using HAS_EXECUTION_LIB")
    add_compile_definitions(HAS_EXECUTION_LIB)
    # Availability of PEXT compiler intrinsic. Not available on all platforms
    message("Using HAS_PEXT")
    add_compile_definitions(HAS_PEXT)

# Apple OS X compiler
elseif (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    message("Compiler Settings: AppleClang")
    set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -Wextra -mpopcnt -mbmi2")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fprofile-instr-generate -fcoverage-mapping -Wpedantic -Wshadow -Wno-conversion -Wunreachable-code -Wuninitialized  -Wold-style-cast -Wunused-variable -Wfloat-equal -Wno-gnu-zero-variadic-macro-arguments")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

    # std::execution lib for parallel std operations. Not available on all platforms
    # message("Using HAS_EXECUTION_LIB")
    # add_compile_definitions(HAS_EXECUTION_LIB)
    # Availability of PEXT compiler intrinsic. Not available on all platforms
    message("Using HAS_PEXT")
    add_compile_definitions(HAS_PEXT)
endif ()

# Print some info to be able to check which compiler has been picked
message("Generator: " ${CMAKE_GENERATOR} " " ${CMAKE_MAKE_PROGRAM})
message("Compiler flags: " ${CMAKE_CXX_FLAGS})
message("Compiler DEBUG flags: " ${CMAKE_CXX_FLAGS_DEBUG})
message("Compiler RELEASE flags: " ${CMAKE_CXX_FLAGS_RELEASE})

# For downloading with GIT
message("Finding GIT...")
set(GIT_EXECUTABLE)
find_package(Git)
if (GIT_FOUND)
    message("GIT found: ${GIT_EXECUTABLE}")
else()
    message(FATAL_ERROR "GIT NOT found:")
endif ()

set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
include(FetchContent)

# Boost - needs to be installed - done via vcpkg
#set(BOOST_DEBUG FALSE)
#set(BOOST_USE_MULTITHREADED TRUE)
#set(BOOST_USE_STATIC_LIBS TRUE)
#set(BOOST_USE_STATIC_RUNTIME FALSE)
#set(BOOST_ALL_NO_LIB 0)
#set(BOOST_LIB_DIAGNOSTIC 1)
#set(Boost_NO_WARN_NEW_VERSIONS 1)
#find_package(Boost 1.72.0 REQUIRED COMPONENTS serialization program_options CONFIG)
#if (Boost_FOUND)
#    message("BOOST found: " ${Boost_ROOT} " " ${Boost_INCLUDE_DIRS})
#    include_directories(${Boost_INCLUDE_DIRS})
#else ()
#    message(FATAL_ERROR "BOOST NOT found:")
#endif ()
#include_directories(${Boost_INCLUDE_DIRS})

#find_package(Boost REQUIRED COMPONENTS program_options)
#target_link_libraries(main PRIVATE Boost::program_options)
#
#find_package(Boost REQUIRED COMPONENTS serialization)
#target_link_libraries(main PRIVATE Boost::serialization)

# Boost - needed for program_options and serialization
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

# Then later in your Boost section:
message("Finding Boost...")
set(Boost_NO_WARN_NEW_VERSIONS 1)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
# Keep the MSVC runtime dynamic (/MD). If you want /MT, set this to ON and
# change your MSVC flags accordingly.
set(Boost_USE_STATIC_RUNTIME OFF)

# Work out include/lib roots from the active vcpkg triplet in this build tree
set(_VCPKG_TRIPLET_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
message(STATUS "Using Boost from ${_VCPKG_TRIPLET_DIR}")
set(BOOST_ROOT "${_VCPKG_TRIPLET_DIR}")
set(Boost_INCLUDE_DIR "${_VCPKG_TRIPLET_DIR}/include")
set(BOOST_LIBRARYDIR "${_VCPKG_TRIPLET_DIR}/lib")

# This is important - use MODULE instead of CONFIG to use CMake's built-in FindBoost
find_package(Boost 1.72.0 REQUIRED COMPONENTS program_options serialization)

if (Boost_FOUND)
    message("BOOST found: version ${Boost_VERSION}")
    message("BOOST include dirs: ${Boost_INCLUDE_DIRS}")
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
else()
    message(FATAL_ERROR "BOOST NOT found")
endif()


# FMT
message("Downloading/Update FMT")
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 8.0.1
)
FetchContent_MakeAvailable(fmt)
include_directories(
        ${fmt_SOURCE_DIR}/include
)

# SPDLOG
message("Downloading/Update SPDLOG")
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.10.0
)
option(SPDLOG_FMT_EXTERNAL "Use external fmt" ON)
set(SPDLOG_FMT_EXTERNAL ON)
FetchContent_MakeAvailable(spdlog)
include_directories(
        ${spdlog_SOURCE_DIR}/include
)

# GOOGLE TEST
message("Downloading/Update Google Test")
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.13.0
)
FetchContent_Declare(googletest)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
option(INSTALL_GMOCK "Install GMock" OFF)
option(INSTALL_GTEST "Install GTest" OFF)
FetchContent_MakeAvailable(googletest)
include_directories(
        ${googletest_SOURCE_DIR}/googletest/include
)

# GOOGLE BENCHMARK
FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.7.1) # need master for benchmark::benchmark
option(BENCHMARK_ENABLE_LTO "Enable LTO" ON)
option(BENCHMARK_ENABLE_GTEST_TESTS "Build benchmark unit tests" OFF)
option(BENCHMARK_ENABLE_TESTING "Enable benchmark tests" OFF)
option(BENCHMARK_ENABLE_INSTALL "Install benchmark" OFF)
FetchContent_MakeAvailable(googlebenchmark)
include_directories(
        ${googlebenchmark_SOURCE_DIR}/include
)

# include sub directories
include_directories(
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/testbench
)

# project sub directories - need to have their own CMakeList.txt
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(testbench)
