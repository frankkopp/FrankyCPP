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
        chesscore/MoveUtils.cpp

        openingbook/OpeningBook.cpp openingbook/OpeningBook.h

        engine/UciHandler.cpp engine/UciHandler.h
        engine/UciOptions.cpp engine/UciOptions.h
        engine/TT.cpp engine/TT.h
        engine/Search.cpp engine/Search.h
        engine/SearchResult.h engine/SearchLimits.h
        engine/SearchStats.h
        engine/See.cpp engine/See.h
        engine/Evaluator.cpp engine/Evaluator.h
        engine/PawnTT.cpp engine/PawnTT.h
        engine/Benchmark.cpp engine/Benchmark.h

        enginetest/TestSuite.cpp enginetest/TestSuite.h
        enginetest/EpdParser.cpp enginetest/EpdParser.h
        enginetest/EdpTest.h
        enginetest/TestTypes.h

        types/attacks.cpp
        types/bitboards.h

        config/ConfigManager.cpp
        config/ConfigGenerators.cpp
        config/ConfigRegistry.cpp

        tablebase/Tablebase.cpp tablebase/Tablebase.h
        tablebase/TablebasePaths.cpp tablebase/TablebasePaths.h
        tablebase/TablebaseDownloader.cpp tablebase/TablebaseDownloader.h
)

add_library(FrankyCPPlib STATIC ${FrankyCPPlib_SRCS})

# SearchTreeSizeTest is a dev/tuning research tool — excluded from production builds.
# It uses CONFIG_OVERRIDE extensively on non-essential (frozen) config members.
# TestSuite/EpdParser stay in all builds as they are used by main.cpp (--testsuite CLI option).
if (NOT FRANKYCPP_PRODUCTION)
    target_sources(FrankyCPPlib PRIVATE
        enginetest/SearchTreeSizeTest.cpp enginetest/SearchTreeSizeTest.h
    )
endif()

# Request C++20 via features (propagates to dependents)
target_compile_features(FrankyCPPlib PUBLIC cxx_std_20)

# Feature macros (target-scoped)
if (FRANKYCPP_PRODUCTION)
    target_compile_definitions(FrankyCPPlib PUBLIC FRANKYCPP_PRODUCTION)
    message(STATUS "FRANKYCPP_PRODUCTION enabled: non-essential configs frozen as static constexpr")
    # Optionally emit MSVC assembly listing files (.asm) for dead-code-elimination verification.
    # Enable with: -DFRANKYCPP_DUMP_ASM=ON  (off by default, MSVC only)
    option(FRANKYCPP_DUMP_ASM "Emit MSVC assembly listings (.asm) for production build verification" OFF)
    if (FRANKYCPP_DUMP_ASM AND MSVC)
        target_compile_options(FrankyCPPlib PRIVATE /FAs)
        message(STATUS "FRANKYCPP_DUMP_ASM enabled: .asm files will be written alongside .obj files")
    endif()
endif()
if (ENABLE_STD_EXECUTION)
    target_compile_definitions(FrankyCPPlib PUBLIC HAS_EXECUTION_LIB)
endif()
if (ENABLE_BMI2_PEXT)
    target_compile_definitions(FrankyCPPlib PUBLIC HAS_PEXT)
    if (MSVC)
        target_compile_options(FrankyCPPlib PUBLIC /arch:AVX2)
    endif()
endif()

# spdlog as header-only with std::format; do not link any spdlog/fmt libs
# Pull only the include dirs from the imported target
# (requires find_package(spdlog CONFIG REQUIRED) in the root CMakeLists)
target_compile_definitions(FrankyCPPlib PUBLIC SPDLOG_HEADER_ONLY=1 SPDLOG_USE_STD_FORMAT=1)

# Target-scoped include directories
# - Expose project src to dependents and tests
# - Expose binary dir for generated version.h
# - Import spdlog includes from its header-only target without linking it
target_include_directories(
        FrankyCPPlib
        PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}
        $<TARGET_PROPERTY:spdlog::spdlog_header_only,INTERFACE_INCLUDE_DIRECTORIES>)

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
# Use vcpkg-configured targets

target_link_libraries(
        FrankyCPPlib
        PUBLIC
        Boost::serialization
        Boost::program_options
        Boost::filesystem
        yaml-cpp::yaml-cpp
        OpenSSL::Crypto
        CURL::libcurl
        PRIVATE
        fathom)

# set the name of the executable
set(exeName FrankyCPP_v${FrankyCPP_VERSION_MAJOR}.${FrankyCPP_VERSION_MINOR})

# define executable
add_executable(${exeName} main.cpp)

# Ensure the executable also requests C++20 explicitly
target_compile_features(${exeName} PRIVATE cxx_std_20)

# Enable Unity/Jumbo build on demand
if (ENABLE_UNITY_BUILD)
    set_target_properties(FrankyCPPlib PROPERTIES UNITY_BUILD ON)
    set_target_properties(${exeName} PROPERTIES UNITY_BUILD ON)
endif()

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

# ============================================================================
# Engine Arena executable
# ============================================================================
message(STATUS "Configuring Engine Arena executable...")
set(arenaExeName ${exeName}_Arena)
message(STATUS "Arena executable name: ${arenaExeName}")

add_executable(${arenaExeName}
    engine_arena_main.cpp
    engine_arena/ArenaConfig.cpp
    engine_arena/ArenaRunner.cpp
    engine_arena/ResultWriter.cpp
    engine_arena/TestSuiteRunner.cpp
    engine_arena/MatchRunner.cpp
    engine_arena/UCIEngine.cpp
    engine_arena/BenchmarkRunner.cpp
)

target_compile_features(${arenaExeName} PRIVATE cxx_std_20)

target_link_libraries(${arenaExeName}
    PRIVATE
    FrankyCPPlib
    yaml-cpp::yaml-cpp
    Boost::program_options
    Boost::filesystem
    nlohmann_json::nlohmann_json
)

target_include_directories(${arenaExeName}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}
)

# Copy config and books for arena executable as well
add_custom_command(
    TARGET ${arenaExeName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/config $<TARGET_FILE_DIR:${arenaExeName}>/config
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/books $<TARGET_FILE_DIR:${arenaExeName}>/books
)

if (ENABLE_UNITY_BUILD)
    set_target_properties(${arenaExeName} PROPERTIES UNITY_BUILD ON)
endif()

if (MSVC AND STRICT_WARNINGS)
    target_compile_options(${arenaExeName} PRIVATE /WX)
endif()

# ============================================================================
# UCIEngine Manual Test executable (optional, for testing external engines)
# ============================================================================
message(STATUS "Configuring UCIEngine Manual Test executable...")
set(uciTestExeName UCIEngine_ManualTest)
message(STATUS "UCIEngine test executable name: ${uciTestExeName}")

add_executable(${uciTestExeName}
    ../test/engine_arena/UCIEngine_ManualTest.cpp
    engine_arena/UCIEngine.cpp
)

target_compile_features(${uciTestExeName} PRIVATE cxx_std_20)

target_link_libraries(${uciTestExeName}
    PRIVATE
    FrankyCPPlib
    Boost::filesystem
)

target_include_directories(${uciTestExeName}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}
)

if (ENABLE_UNITY_BUILD)
    set_target_properties(${uciTestExeName} PROPERTIES UNITY_BUILD ON)
endif()

if (MSVC AND STRICT_WARNINGS)
    target_compile_options(${uciTestExeName} PRIVATE /WX)
endif()

# ============================================================================

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

# Release packaging via install command
# Creates versioned Release/FrankyCPP_v<version>/ folder and ZIP with filtered books
set(RELEASE_VERSION "v${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/Release/FrankyCPP_${RELEASE_VERSION}")
set(RELEASE_ZIP "${PROJECT_SOURCE_DIR}/Release/FrankyCPP_${RELEASE_VERSION}.zip")

install(CODE "
    message(STATUS \"===========================================\")
    message(STATUS \"Creating release package for FrankyCPP ${RELEASE_VERSION}...\")
    message(STATUS \"===========================================\")

    # Build fresh Release executable
    message(STATUS \"Rebuilding for fresh release...\")
    execute_process(
        COMMAND \"${CMAKE_COMMAND}\" --build \"${CMAKE_BINARY_DIR}\" --config Release --clean-first
        RESULT_VARIABLE _build_result
        OUTPUT_VARIABLE _build_output
        ERROR_VARIABLE _build_error
    )

    # Check if executables exist after build
    set(_exe_path \"$<TARGET_FILE:${exeName}>\")
    set(_arena_exe_path \"$<TARGET_FILE:${arenaExeName}>\")
    if(NOT EXISTS \"\${_exe_path}\")
        if(_build_result)
            message(WARNING \"Build failed with error code: \${_build_result}\")
            message(WARNING \"Build output: \${_build_output}\")
            message(WARNING \"Build error: \${_build_error}\")
        endif()
        message(WARNING \"Executable not found at: \${_exe_path}\")
        message(WARNING \"Please build the project manually first: cmake --build ${CMAKE_BINARY_DIR} --config Release\")
        message(FATAL_ERROR \"Cannot create release package without executable\")
    else()
        message(STATUS \"Fresh executable built successfully\")
    endif()

    if(NOT EXISTS \"\${_arena_exe_path}\")
        message(WARNING \"Arena executable not found at: \${_arena_exe_path}\")
        message(WARNING \"Arena executable will not be included in the release package\")
    else()
        message(STATUS \"Arena executable found\")
    endif()

    # Clean up old release folder and ZIP
    message(STATUS \"Cleaning previous release...\")
    file(REMOVE_RECURSE \"${RELEASE_DIR}\")
    file(REMOVE \"${RELEASE_ZIP}\")

    # Create directory structure
    message(STATUS \"Creating directory structure...\")
    file(MAKE_DIRECTORY \"${RELEASE_DIR}\")
    file(MAKE_DIRECTORY \"${RELEASE_DIR}/books\")
    file(MAKE_DIRECTORY \"${RELEASE_DIR}/config\")

    # Copy executable
    message(STATUS \"Copying executable...\")
    file(INSTALL \"$<TARGET_FILE:${exeName}>\"
         DESTINATION \"${RELEASE_DIR}\"
         FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

    # Copy Arena executable (if it exists)
    if(EXISTS \"$<TARGET_FILE:${arenaExeName}>\")
        message(STATUS \"Copying Arena executable...\")
        file(INSTALL \"$<TARGET_FILE:${arenaExeName}>\"
             DESTINATION \"${RELEASE_DIR}\"
             FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    endif()

    # Copy config files
    message(STATUS \"Copying config files...\")
    file(INSTALL \"${PROJECT_SOURCE_DIR}/config/\"
         DESTINATION \"${RELEASE_DIR}/config\"
         FILES_MATCHING PATTERN \"*\")

    # Copy books with filtering
    message(STATUS \"Copying book files...\")
    execute_process(
        COMMAND \"${CMAKE_COMMAND}\"
            -DSOURCE_DIR=${PROJECT_SOURCE_DIR}/books
            -DDEST_DIR=${RELEASE_DIR}/books
            -P ${PROJECT_SOURCE_DIR}/cmake/CopyBooksFiltered.cmake
        RESULT_VARIABLE _copy_result
    )
    if(_copy_result)
        message(WARNING \"Book file filtering had issues\")
    endif()

    # Create ZIP archive
    message(STATUS \"Creating ZIP archive...\")
    execute_process(
        COMMAND \"${CMAKE_COMMAND}\" -E chdir \"${RELEASE_DIR}\"
            \"${CMAKE_COMMAND}\" -E tar cfv \"${RELEASE_ZIP}\" --format=zip .
        RESULT_VARIABLE _zip_result
        OUTPUT_QUIET
    )

    if(_zip_result)
        message(WARNING \"ZIP creation failed\")
    else()
        message(STATUS \"===========================================\")
        message(STATUS \"Release package created successfully!\")
        message(STATUS \"  Folder: ${RELEASE_DIR}\")
        message(STATUS \"  ZIP:    ${RELEASE_ZIP}\")
        message(STATUS \"===========================================\")
    endif()
    "
    COMPONENT Runtime
)

# Note: No GoogleTest includes are injected into production targets anymore.
