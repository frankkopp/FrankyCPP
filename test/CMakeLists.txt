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
        engine/SeeTest.cpp
        engine/EvaluatorTest.cpp
        engine/PawnTT_Test.cpp
        engine/EngineSpeedTests.cpp
        engine/PVTableTest.cpp
        engine/VariationStackTest.cpp
        engine/BenchmarkTest.cpp

        enginetest/SearchTreeSizeTest_Test.cpp
        enginetest/TestSuite_Test.cpp
        enginetest/EpdParser_Test.cpp
        enginetest/EdpTest_Test.cpp
        enginetest/TestTypes_Test.cpp

        tablebase/TablebaseTest.cpp
        tablebase/TablebasePathsTest.cpp

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


# Ensure unit tests explicitly request C++20
target_compile_features(${testExeName} PRIVATE cxx_std_20)

# Enable Unity/Jumbo build opt-in
if (ENABLE_UNITY_BUILD)
    set_target_properties(${testExeName} PROPERTIES UNITY_BUILD ON)
endif()

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
endif()

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
gtest_discover_tests(${testExeName})
