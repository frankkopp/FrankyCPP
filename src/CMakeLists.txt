message("====================================================================")
message("FrankyCPP src/")
message("====================================================================")

cmake_minimum_required(VERSION 3.16)

# define library files
set(FrankyCPPlib_SRCS

        common/ThreadPool.cpp
        common/Logging.cpp

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
        engine/SearchStats.h
        engine/See.cpp engine/See.h
        engine/Evaluator.cpp engine/Evaluator.h engine/EvalConfig.h
        engine/PawnTT.cpp engine/PawnTT.h

        enginetest/SearchTreeSizeTest.cpp enginetest/SearchTreeSizeTest.h
        enginetest/TestSuite.cpp enginetest/TestSuite.h
        )

add_library(FrankyCPPlib STATIC ${FrankyCPPlib_SRCS})

# Feature macros (target-scoped)
if (ENABLE_STD_EXECUTION)
    target_compile_definitions(FrankyCPPlib PUBLIC HAS_EXECUTION_LIB)
endif()
if (ENABLE_BMI2_PEXT)
    target_compile_definitions(FrankyCPPlib PUBLIC HAS_PEXT)
    if (MSVC)
        target_compile_options(FrankyCPPlib PUBLIC /arch:AVX2)
    endif()
endif()

# Target-scoped include directories
# - Expose project src to dependents and tests
# - Expose binary dir for generated version.h
target_include_directories(
        FrankyCPPlib
        PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}
)

# pre compile main header files (optional)
if (FRANKYCPP_USE_PCH)
    target_precompile_headers(
            FrankyCPPlib PUBLIC
            init.h
            types/types.h
            common/misc.h
            common/stringutil.h
            common/ThreadPool.h
            common/Logging.h
            chesscore/Values.h
            chesscore/Position.h
            chesscore/MoveGenerator.h
            chesscore/Perft.h
            chesscore/History.h
    )
endif()

# Apply AVX2 if explicitly requested
if (ENABLE_AVX2 AND MSVC)
    target_compile_options(FrankyCPPlib PUBLIC /arch:AVX2)
endif()

# Explicitly link Boost components via targets rather than ${Boost_LIBRARIES}
# to avoid accidental dynamic linkage
# Use non-namespaced targets from FetchContent builds (pinned versions)

target_link_libraries(
        FrankyCPPlib
        PUBLIC
        fmt
        spdlog
        Boost::serialization
        Boost::program_options
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

# The executable may need access to generated headers
target_include_directories(
        ${exeName}
        PRIVATE
        ${PROJECT_BINARY_DIR}
)

# STRICT_WARNINGS for CI: treat warnings as errors when enabled
if (MSVC AND STRICT_WARNINGS)
    target_compile_options(FrankyCPPlib PRIVATE /WX)
    target_compile_options(${exeName} PRIVATE /WX)
endif()

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

# Note: No GoogleTest includes are injected into production targets anymore.
