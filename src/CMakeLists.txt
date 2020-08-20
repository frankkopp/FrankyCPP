message("====================================================================")
message("FrankyCPP src/")
message("====================================================================")

cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 20)

# define library files
set(FrankyCPPlib_SRCS
        types/bitboard.cpp

        common/ThreadPool.cpp
        common/Logging.cpp

        chesscore/Values.cpp
        chesscore/Position.cpp
        chesscore/MoveGenerator.cpp
        chesscore/Perft.cpp
        chesscore/History.cpp                       

        openingbook/OpeningBook.cpp openingbook/OpeningBook.h

        engine/UciHandler.cpp engine/UciHandler.h
        engine/UciOptions.cpp engine/UciOptions.h
        engine/TT.cpp engine/TT.h
        engine/Search.cpp engine/Search.h
        engine/SearchResult.h engine/SearchLimits.h engine/SearchConfig.h
        engine/SearchStats.cpp engine/SearchStats.h
        engine/See.cpp engine/See.h
        engine/Evaluator.cpp engine/Evaluator.h engine/EvalConfig.h
        engine/PawnTT.cpp engine/PawnTT.h

        enginetest/SearchTreeSizeTest.cpp enginetest/SearchTreeSizeTest.h
        enginetest/TestSuite.cpp enginetest/TestSuite.h
        )

add_library(FrankyCPPlib STATIC ${FrankyCPPlib_SRCS})

# pre compile main header files
target_precompile_headers(
        FrankyCPPlib PUBLIC
        init.h
        types/types.h
        common/misc.h
        common/stringutil.h
        common/Semaphore.h
        common/ThreadPool.h
        common/Logging.h
        chesscore/Values.h
        chesscore/Position.h
        chesscore/MoveGenerator.h
        chesscore/Perft.h
        chesscore/History.h
)

target_link_libraries(
        FrankyCPPlib
        PUBLIC
        fmt
        spdlog
        ${Boost_LIBRARIES}
)

# set the name of the executable
set(exeName FrankyCPP_v${FrankyCPP_VERSION_MAJOR}.${FrankyCPP_VERSION_MINOR})

# define executable
add_executable(${exeName} main.cpp)

# link executable with library
target_link_libraries(
        ${exeName}
        PUBLIC
        FrankyCPPlib          
)

# copy config files and opening books to the build directories
add_custom_command(
        TARGET ${exeName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/config $<TARGET_FILE_DIR:${exeName}>/config
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/books $<TARGET_FILE_DIR:${exeName}>/books
)

# install executable, config and opening books into the release folder
install(TARGETS ${exeName}
        CONFIGURATIONS Release RelWithDebInfo
        RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/Release/bin
        )
install(DIRECTORY ${PROJECT_SOURCE_DIR}/config
        DESTINATION ${PROJECT_SOURCE_DIR}/Release/bin
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/books
        DESTINATION ${PROJECT_SOURCE_DIR}/Release/bin
        PATTERN "*.cache.*" EXCLUDE
)

