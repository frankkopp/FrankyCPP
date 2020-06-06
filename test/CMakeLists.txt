message("====================================================================")
message("FrankyCPP test/")
message("====================================================================")

cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)

# set the name of the executable
set(testExeName FrankyCPP_v${FrankyCPP_VERSION_MAJOR}.${FrankyCPP_VERSION_MINOR}_Test)

include(GoogleTest)
add_executable(
        ${testExeName}
        TypesTest.cpp
        BitboardTest.cpp
        PositionTest.cpp
        ValuesTest.cpp
        BOOST_Test.cpp
        TimingTests.cpp
)

target_link_libraries(
        ${testExeName}
        PUBLIC
        FrankyCPPlib
        gtest
        gtest_main
)

# copy config files and opening books to the build directories
#add_custom_command(
#        TARGET ${testExeName} POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/config $<TARGET_FILE_DIR:${testExeName}>/config
#        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/books $<TARGET_FILE_DIR:${testExeName}>/books
#)

gtest_discover_tests(${testExeName})

