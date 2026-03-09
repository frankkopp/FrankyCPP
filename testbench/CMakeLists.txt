message("====================================================================")
message("FrankyCPP testbench/")
message("====================================================================")

cmake_minimum_required(VERSION 3.16)

# Google Benchmark via vcpkg
find_package(benchmark CONFIG REQUIRED)

# Google Benchmark executable
set(benchExeName FrankyCPP_v${FrankyCPP_VERSION_MAJOR}.${FrankyCPP_VERSION_MINOR}_Bench)

# Auto-discover benchmark source files (CONFIGURE_DEPENDS
# ensures CMake re-runs when files are added or removed)
file(GLOB BENCH_SRCS CONFIGURE_DEPENDS *.cpp)

add_executable(
        ${benchExeName}
        ${BENCH_SRCS}
)

# Ensure benchmark explicitly requests C++20
target_compile_features(${benchExeName} PRIVATE cxx_std_20)

# Enable Unity/Jumbo build opt-in
if (ENABLE_UNITY_BUILD)
    set_target_properties(${benchExeName} PROPERTIES UNITY_BUILD ON)
endif ()

# Ensure access to generated headers (e.g., version.h)
target_include_directories(
        ${benchExeName}
        PRIVATE
        ${PROJECT_BINARY_DIR}
)

# STRICT_WARNINGS for CI: treat warnings as errors when enabled
if (MSVC AND STRICT_WARNINGS)
    target_compile_options(${benchExeName} PRIVATE /WX)
endif ()

# Link against project lib and vcpkg-provided Google Benchmark target
target_link_libraries(
        ${benchExeName}
        PUBLIC
        FrankyCPPlib
        benchmark::benchmark
)
