#!/usr/bin/bash

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBENCHMARK_ENABLE_TESTING=OFF -G Ninja ..
ninja -j 8
ctest -C Release -DBENCHMARK_ENABLE_TESTING=OFF -E ".*SpeedTests.*" -E ".*TimingTests.*" --output-on-failure
./testbench/FrankyCPP_v0.3_Bench
