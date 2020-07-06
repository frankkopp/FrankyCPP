message("====================================================================")
message("FrankyCPP Main")
message("CMAKE " ${CMAKE_VERSION} " " ${CMAKE_COMMAND})
message("====================================================================")

cmake_minimum_required(VERSION 3.13)

# Project name and version
project(FrankyCPP VERSION 0.2)
message("FrankyCPP v" ${PROJECT_VERSION_MAJOR} "." ${PROJECT_VERSION_MINOR})

# updates template file and replaces placeholder variables
configure_file(src/version.h.in version.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# C++ settings
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Unit testing enabled
enable_testing()

# Platform
message("Recognized platform is " ${CMAKE_SYSTEM_NAME} " " ${CMAKE_SYSTEM_VERSION})

# Compiler Settings
message("Compiler is " ${CMAKE_CXX_COMPILER_ID} " " ${CMAKE_CXX_COMPILER_VERSION} " " ${CMAKE_CXX_COMPILER})

if (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    message("Compiler Settings: AppleClang")
    set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -Wextra -mpopcnt -mbmi2")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fprofile-instr-generate -fcoverage-mapping -Wpedantic -Wshadow -Wno-conversion -Wunreachable-code -Wuninitialized  -Wold-style-cast -Wunused-variable -Wfloat-equal -Wno-gnu-zero-variadic-macro-arguments")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "11")
        #        message("Using HAS_EXECUTION_LIB")
        #        add_compile_definitions(HAS_EXECUTION_LIB)
        message("Using HAS_PEXT")
        add_compile_definitions(HAS_PEXT)
    endif ()

elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message("Compiler Settings: Clang")
    set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -Wextra -mpopcnt -mbmi2")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fprofile-instr-generate -fcoverage-mapping -Wpedantic -Wshadow -Wno-conversion -Wunreachable-code -Wuninitialized  -Wold-style-cast -Wunused-variable -Wfloat-equal -Wno-gnu-zero-variadic-macro-arguments")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "9")
        message("Using HAS_EXECUTION_LIB")
        add_compile_definitions(HAS_EXECUTION_LIB)
        message("Using HAS_PEXT")
        add_compile_definitions(HAS_PEXT)
    endif ()

elseif (CMAKE_COMPILER_IS_GNUCXX)
    message("Compiler Settings: GNU")
    set(CMAKE_CXX_FLAGS "-std=c++17 -m64 -mpopcnt -mbmi2 -Wall -Wextra -Wpedantic -Wno-error=pedantic -Wshadow -Wno-unknown-pragmas -Wno-variadic-macros")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 --coverage -Wno-unused-parameter -Wno-unused-variable")
    # additional: -Wno-unknown-pragmas -Weffc++ -Wimplicit-fallthrough -Wswitch -Wpointer-arith -Wcast-qual -Wconversion -Wno-sign-conversion -Wno-error=padded -Wno-error=inline")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "9.2")
        #message("Using HAS_EXECUTION_LIB")
        #add_compile_definitions(HAS_EXECUTION_LIB)
        message("Using HAS_PEXT")
        add_compile_definitions(HAS_PEXT)
    endif ()

elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    message("Compiler Settings: MSVC")
    set(CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /W4 /GR /EHsc /std:c++17 /wd4127 /arch:AVX2")
    set(CMAKE_CXX_FLAGS_DEBUG " /MDd /Zi /Ob0 /Od /RTC1")
    # /showIncludes
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "19.24")
        message("Using HAS_EXECUTION_LIB")
        add_compile_definitions(HAS_EXECUTION_LIB)
        message("Using HAS_PEXT")
        add_compile_definitions(HAS_PEXT)
    endif ()
endif ()

message("Compiler flags: " ${CMAKE_CXX_FLAGS})
message("Compiler DEBUG flags: " ${CMAKE_CXX_FLAGS_DEBUG})
message("Compiler RELEASE flags: " ${CMAKE_CXX_FLAGS_RELEASE})
message("EXEC: " ${CMAKE_CXX_LINK_EXECUTABLE})

# For downloading with GIT
message("Finding GIT...")
set(GIT_EXECUTABLE)
find_package(Git)
if (GIT_FOUND)
    message("git found: ${GIT_EXECUTABLE}")
elseif ()
    message("git NOT found:")
endif ()

set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# Getting included libs from Github
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.10.0
)
message("Downloading/Update Google Test")
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_GetProperties(googletest)
if (NOT googletest_POPULATED)
    # Fetch the content using previously declared details
    FetchContent_Populate(googletest)
    # Set custom variables, policies, etc.
    option(INSTALL_GMOCK "Install Googletest's GMock?" OFF)
    option(INSTALL_GTEST "Install Googletest's GTest?" OFF)
    # Bring the populated content into the build
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()

FetchContent_MakeAvailable(googletest)
include_directories(
        ${googletest_SOURCE_DIR}/googletest/include
)

## Getting included libs from Github
include(FetchContent)
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 6.1.2
)
message("Downloading/Update FMT")
FetchContent_MakeAvailable(fmt)
include_directories(
        ${fmt_SOURCE_DIR}/include
)

# Getting included libs from Github
include(FetchContent)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.5.0
)
message("Downloading/Update SPDLOG")
option(SPDLOG_FMT_EXTERNAL ON)
FetchContent_MakeAvailable(spdlog)
include_directories(
        ${spdlog_SOURCE_DIR}/include
)

# BOOST
if (APPLE)
    message("Find Boost on Apple")
    set(BOOST_ROOT "/usr/local/boost_1_73_0")
    set(Boost_INCLUDE_DIR "/usr/local/boost_1_73_0")
elseif (CYGWIN)
    message("Find Boost on Cygwin")
    set(BOOST_ROOT "/usr/local/boost_1_72_0")
    set(Boost_INCLUDE_DIR "/usr/local/boost_1_72_0")
elseif (WIN32)
    message("Find Boost on Win32")
    set(BOOST_ROOT "C:/Boost")
    set(BOOST_INCLUDEDIR "C:/Boost/include/boost-1_73")
    set(BOOST_LIBRARYDIR "C:/Boost/lib")
else ()
    message("Find Boost on other")
    set(BOOST_ROOT "/usr/local/boost_1_73_0")
    set(Boost_INCLUDE_DIR "/usr/local/boost_1_73_0")
endif ()
set(Boost_DEBUG FALSE)
set(Boost_USE_MULTITHREADED TRUE)
set(Boost_USE_STATIC_LIBS TRUE)
set(Boost_USE_STATIC_RUNTIME FALSE)
find_package(Boost 1.72.0 COMPONENTS serialization program_options timer log log_setup REQUIRED)
if (Boost_FOUND)
    message("BOOST found: " ${BOOST_ROOT} " " ${Boost_INCLUDE_DIR})
    include_directories(${Boost_INCLUDE_DIRS})
endif ()
set(BOOST_ALL_NO_LIB 0)
set(BOOST_LIB_DIAGNOSTIC 1)

# My project
include_directories(
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/test
)

# project sub directories - need to have their own CMakeList.txt
add_subdirectory(src)
add_subdirectory(test)



