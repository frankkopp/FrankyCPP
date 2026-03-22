message("====================================================================")
message("FrankyCPP test/")
message("====================================================================")

cmake_minimum_required(VERSION 3.16)

# GoogleTest via vcpkg
find_package(GTest CONFIG REQUIRED)

# Google Test executable
set(testExeName FrankyCPP_v${FrankyCPP_VERSION_MAJOR}.${FrankyCPP_VERSION_MINOR}_Test)
include(GoogleTest)

# Auto-discover test source files from each module (CONFIGURE_DEPENDS
# ensures CMake re-runs when files are added or removed)
file(GLOB TEST_CONFIGURATION CONFIGURE_DEPENDS config/*.cpp)
file(GLOB TEST_TYPES CONFIGURE_DEPENDS types/*.cpp)
file(GLOB TEST_COMMON CONFIGURE_DEPENDS common/*.cpp common/pgn/*.cpp)
file(GLOB TEST_CHESSCORE CONFIGURE_DEPENDS chesscore/*.cpp)
file(GLOB TEST_ENGINE CONFIGURE_DEPENDS engine/*.cpp)
file(GLOB TEST_ENGINETEST CONFIGURE_DEPENDS enginetest/*.cpp)
file(GLOB TEST_TABLEBASE CONFIGURE_DEPENDS tablebase/*.cpp)
file(GLOB TEST_OPENINGBOOK CONFIGURE_DEPENDS openingbook/*.cpp)
file(GLOB TEST_ENGINE_ARENA CONFIGURE_DEPENDS engine_arena/*.cpp)
file(GLOB TEST_CLI CONFIGURE_DEPENDS cli/*.cpp)

# Exclude manual test (standalone executable, not a GTest)
list(FILTER TEST_ENGINE_ARENA EXCLUDE REGEX "UCIEngine_ManualTest\\.cpp$")

# Engine arena source files needed by tests (compiled directly from src/)
file(GLOB SRCS_ENGINE_ARENA_FOR_TESTS CONFIGURE_DEPENDS ../src/engine_arena/*.cpp)
# ArenaRunner is only needed by the arena executable, not tests
list(FILTER SRCS_ENGINE_ARENA_FOR_TESTS EXCLUDE REGEX "ArenaRunner\\.cpp$")

add_executable(
        ${testExeName}

        ${TEST_TYPES}
        ${TEST_COMMON}
        ${TEST_CHESSCORE}
        ${TEST_OPENINGBOOK}
        ${TEST_ENGINE}
        ${TEST_ENGINETEST}
        ${TEST_TABLEBASE}
        ${TEST_ENGINE_ARENA}
        ${TEST_CONFIGURATION}
        ${TEST_CLI}

        # Engine arena source files needed by tests
        ${SRCS_ENGINE_ARENA_FOR_TESTS}

        # CrashHandler for minidump generation on crashes
        ../src/common/CrashHandler.cpp

        TimingTests.cpp
)

# Ensure unit tests explicitly request C++20
target_compile_features(${testExeName} PRIVATE cxx_std_20)

# Enable Unity/Jumbo build opt-in
if (ENABLE_UNITY_BUILD)
    set_target_properties(${testExeName} PROPERTIES UNITY_BUILD ON)
endif ()

# Ensure access to generated headers (e.g., version.h) and local test headers
target_include_directories(
        ${testExeName}
        PRIVATE
        ${PROJECT_BINARY_DIR}
        ${PROJECT_SOURCE_DIR}/test
        ${PROJECT_SOURCE_DIR}/src)  # Add src directory for engine_arena headers

# STRICT_WARNINGS for CI: treat warnings as errors when enabled
if (MSVC AND STRICT_WARNINGS)
    target_compile_options(${testExeName} PRIVATE /WX)
endif ()

# Link against project lib and vcpkg-provided GoogleTest targets
# Also link yaml-cpp for config YAML parsing in smoke test
# Also link nlohmann_json for engine_arena components
target_link_libraries(
        ${testExeName}
        PUBLIC
        FrankyCPPlib
        GTest::gtest
        GTest::gtest_main
        yaml-cpp::yaml-cpp
        nlohmann_json::nlohmann_json
)

# copy config files and opening books to the build directories
add_custom_command(
        TARGET ${testExeName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/config $<TARGET_FILE_DIR:${testExeName}>/config
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/books $<TARGET_FILE_DIR:${testExeName}>/books
)

# Ensure main executable is built before tests (needed for CLI integration tests)
set(mainExeName FrankyCPP_v${FrankyCPP_VERSION_MAJOR}.${FrankyCPP_VERSION_MINOR})
add_dependencies(${testExeName} ${mainExeName})

# Register tests via CMake's GoogleTest module
# Note: gtest_discover_tests runs the executable at build time to enumerate tests.
# This fails with TSAN on WSL2 due to ASLR memory mapping issues.
# When TSAN is enabled, we skip auto-discovery and run tests manually.
if (ENABLE_TSAN)
    message(STATUS "TSAN enabled: Skipping gtest_discover_tests (run tests manually)")
    message(STATUS "  To run tests: setarch \$(uname -m) -R ./test/${testExeName}")
    message(STATUS "  Or: sudo sysctl -w kernel.randomize_va_space=0 && ./test/${testExeName}")
    # Add a simple CTest target that reminds user how to run
    add_test(NAME TSAN_RUN_MANUALLY
            COMMAND ${CMAKE_COMMAND} -E echo "Run TSAN tests manually: setarch \$(uname -m) -R ./test/${testExeName}")
else ()
    gtest_discover_tests(${testExeName})
endif ()
