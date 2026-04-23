// FrankyCPP
// Copyright (c) 2018-2026 Frank Kopp
//
// MIT License
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


//=============================================================================
// UCIEngine_ManualTest.cpp - Manual test for UCIEngine
//=============================================================================
//
// Simple test program to validate UCIEngine functionality.
// Run this manually to test UCI communication with an external engine.
//
// Usage:
//   1. Build the test: (compile with arena sources)
//   2. Run: UCIEngine_ManualTest.exe <path-to-engine>
//
// Example positions tested:
//   - Starting position
//   - Tactical position (Qa4+ move)
//
//=============================================================================

#include "engine_arena/UCIEngine.h"
#include <exception>
#include <iostream>

using namespace arena;
using namespace chess;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <engine-path>" << std::endl;
    std::cerr << "Example: " << argv[0] << " Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe" << std::endl;
    return 1;
  }

  const std::string enginePath = argv[1];

  try {
    std::cout << "========================================" << std::endl;
    std::cout << "UCIEngine Manual Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Engine: " << enginePath << std::endl;
    std::cout << std::endl;

    // Create engine instance (reused for all tests)
    std::cout << "Starting engine..." << std::endl;
    UCIEngine engine(enginePath);
    std::cout << "Engine name: " << engine.getEngineName() << std::endl;
    std::cout << std::endl;

    // Test 1: Starting position
    std::cout << "Test 1: Starting position" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    engine.newGame(); // Clear state before first position

    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    if (!engine.setPosition(startFen)) {
      std::cerr << "ERROR: Failed to set position" << std::endl;
      return 1;
    }

    UCISearchResult result1 = engine.search(milliseconds{1000}, static_cast<Depth>(10));

    if (result1.bestMove.empty()) {
      std::cerr << "ERROR: No best move returned" << std::endl;
      return 1;
    }

    std::cout << "Best move: " << result1.bestMove << std::endl;
    std::cout << "Depth:     " << result1.depth << std::endl;
    std::cout << "Nodes:     " << result1.nodes << std::endl;
    std::cout << "Score:     " << result1.score << " cp" << std::endl;
    std::cout << "Time:      " << result1.time.count() << " ms" << std::endl;
    std::cout << std::endl;

    // Test 2: Tactical position (mate in 2)
    std::cout << "Test 2: Tactical position (Qa4+ leads to mate)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    engine.newGame(); // Clear state before new position (isolate from Test 1)

    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string tacticalFen = "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4";

    if (!engine.setPosition(tacticalFen)) {
      std::cerr << "ERROR: Failed to set position" << std::endl;
      return 1;
    }

    UCISearchResult result2 = engine.search(milliseconds{3000}, static_cast<Depth>(20));

    if (result2.bestMove.empty()) {
      std::cerr << "ERROR: No best move returned" << std::endl;
      return 1;
    }

    std::cout << "Best move: " << result2.bestMove << std::endl;
    std::cout << "Depth:     " << result2.depth << std::endl;
    std::cout << "Nodes:     " << result2.nodes << std::endl;
    std::cout << "Score:     " << result2.score << " cp" << std::endl;
    std::cout << "Time:      " << result2.time.count() << " ms" << std::endl;
    std::cout << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "All tests passed!" << std::endl;
    std::cout << "Note: Engine process was reused for both tests" << std::endl;
    std::cout << "      newGame() was called to clear state between tests" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
}
