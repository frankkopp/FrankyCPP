message("====================================================================")
message("FrankyCPP test/")
message("====================================================================")

cmake_minimum_required(VERSION 3.16)

# GoogleTest via vcpkg
find_package(GTest CONFIG REQUIRED)

# Google Test executable
set(testExeName FrankyCPP_v${FrankyCPP_VERSION_MAJOR}.${FrankyCPP_VERSION_MINOR}_Test)
include(GoogleTest)
add_executable(
        ${testExeName}

        types/TypesTest.cpp
        types/BitboardTest.cpp
        types/ValuesTest.cpp
        types/FileRankTest.cpp
        types/ColorTest.cpp
        types/SquareIteratorTest.cpp
        types/StaticMoveListTest.cpp

        common/FifoTest.cpp
        common/ThreadPoolTest.cpp
        common/StringUtilsTest.cpp
        common/TimeUtilsTest.cpp
        common/LoggingTest.cpp
        common/MiscTest.cpp

        chesscore/PositionTest.cpp
        chesscore/MoveGeneratorTest.cpp
        chesscore/PerftTest.cpp
        chesscore/SpeedTests.cpp
        chesscore/MoveUtils_Test.cpp

        openingbook/OpeningbookTest.cpp

        engine/UCITests.cpp
        engine/UciOptionsTest.cpp
        engine/TT_Test.cpp
        engine/SearchTest.cpp
        engine/SearchSmpTest.cpp
        engine/SeeTest.cpp
        engine/EvaluatorTest.cpp
        engine/PawnTT_Test.cpp
        engine/EngineSpeedTests.cpp
        engine/PVTableTest.cpp
        engine/VariationStackTest.cpp
        engine/BenchmarkTest.cpp

        enginetest/TestSuite_Test.cpp
        enginetest/EpdParser_Test.cpp
        enginetest/EdpTest_Test.cpp
        enginetest/TestTypes_Test.cpp

        tablebase/TablebaseTest.cpp
        tablebase/TablebasePathsTest.cpp
        tablebase/TablebaseDownloaderTest.cpp

        engine_arena/TestSuiteRunner_IntegrationTest.cpp
        engine_arena/UCIEngine_ErrorHandlingTest.cpp
        engine_arena/UCIEngine_OptionsTest.cpp

        # Engine arena source files needed by tests
        ../src/engine_arena/ArenaConfig.cpp
        ../src/engine_arena/TestSuiteRunner.cpp
        ../src/engine_arena/UCIEngine.cpp
        ../src/engine_arena/ResultWriter.cpp
        ../src/engine_arena/MatchRunner.cpp
        ../src/engine_arena/BenchmarkRunner.cpp

        # CrashHandler for minidump generation on crashes
        ../src/common/CrashHandler.cpp

        TimingTests.cpp

        config/ConfigYamlSmokeTest.cpp
        config/ConfigDataTests.cpp
        config/ConfigManagerTests.cpp
        config/ConfigAutoInitTests.cpp
        config/ConfigRegistryTest.cpp
        config/ConfigGeneratorsTest.cpp
        engine_arena/ResultWriter_Test.cpp
        engine_arena/MatchRunner_Test.cpp

        cli/CliIntegrationTest.cpp)

# SearchTreeSizeTest_Test is a dev/tuning test — excluded from production builds.
# Mirrors the exclusion of SearchTreeSizeTest.cpp from FrankyCPPlib in src/CMakeLists.txt.
if (NOT FRANKYCPP_PRODUCTION)
    target_sources(${testExeName} PRIVATE
            enginetest/SearchTreeSizeTest_Test.cpp
    )
endif ()


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
