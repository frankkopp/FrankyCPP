message("====================================================================")
message("FrankyCPP test/")
message("====================================================================")

cmake_minimum_required(VERSION 3.13)
set(CMAKE_CXX_STANDARD 20)

# set the name of the executable
set(testExeName FrankyCPP_v${FrankyCPP_VERSION_MAJOR}.${FrankyCPP_VERSION_MINOR}_Test)

include(GoogleTest)
add_executable(
        ${testExeName}

        types/TypesTest.cpp
        types/BitboardTest.cpp
        types/ValuesTest.cpp

        common/SemaphoreTest.cpp
        common/FifoTest.cpp
        common/ThreadPoolTest.cpp

        chesscore/PositionTest.cpp
        chesscore/MoveGeneratorTest.cpp
        chesscore/PerftTest.cpp
        chesscore/SpeedTests.cpp

        openingbook/PGN_ReaderTest.cpp
        openingbook/OpeningBookTest.cpp

        engine/UCITests.cpp
        engine/UciOptionsTest.cpp
        engine/TT_Test.cpp
        engine/SearchTest.cpp
        engine/SeeTest.cpp
        engine/EvaluatorTest.cpp
        engine/PawnTT_Test.cpp
        engine/EngineSpeedTests.cpp

        enginetest/SearchTreeSizeTest_Test.cpp
        enginetest/TestSuite_Test.cpp

        TimingTests.cpp
        BOOST_Test.cpp
        )

target_link_libraries(
        ${testExeName}
        PUBLIC
        FrankyCPPlib
        gtest
        gtest_main
)

# copy config files and opening books to the build directories
add_custom_command(
        TARGET ${testExeName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/config $<TARGET_FILE_DIR:${testExeName}>/config
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/books $<TARGET_FILE_DIR:${testExeName}>/books
)

gtest_discover_tests(${testExeName})

