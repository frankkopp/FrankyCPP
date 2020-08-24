message("====================================================================")
message("FrankyCPP testbench/")
message("====================================================================")

cmake_minimum_required(VERSION 3.16)

# Google Benchmark executable
set(benchExeName FrankyCPP_v${FrankyCPP_VERSION_MAJOR}.${FrankyCPP_VERSION_MINOR}_Bench)
add_executable(
        ${benchExeName}
        ChessCoreBench.cpp
        TimingBench.cpp
        RunBench.cpp)
target_link_libraries(
        ${benchExeName}
        PUBLIC
        FrankyCPPlib
        benchmark
        benchmark_main
)

