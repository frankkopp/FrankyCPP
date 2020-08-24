/*
 * MIT License
 *
 * Copyright (c) 2018 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "chesscore/Position.h"

#include <benchmark/benchmark.h>

static void BM_SetupPosition(benchmark::State& state) {
Position position{};
const char* const fen = "r3k2r/1ppn3p/4q1n1/8/4Pp2/3R4/p1p2PPP/R5K1 b kq e3 0 1";
  for (auto _ : state) {
    position = Position(fen);
  }
}
// Register the function as a benchmark
BENCHMARK(BM_SetupPosition);

static void BM_DoUndoMove(benchmark::State& state) {
  Position position("r3k2r/1ppn3p/4q1n1/8/4Pp2/3R4/p1p2PPP/R5K1 b kq e3 0 1");
  const Move move1  = createMove(SQ_F4, SQ_E3, ENPASSANT);
  const Move move2  = createMove(SQ_F2, SQ_E3);
  const Move move3  = createMove(SQ_E8, SQ_G8, CASTLING);
  const Move move4  = createMove(SQ_D3, SQ_C3);
  const Move move5  = createMove(SQ_C2, SQ_C1, PROMOTION, QUEEN);
  for (auto _ : state) {
    position.doMove(move1);
    position.doMove(move2);
    position.doMove(move3);
    position.doMove(move4);
    position.doMove(move5);
    position.undoMove();
    position.undoMove();
    position.undoMove();
    position.undoMove();
    position.undoMove();
  }
}
// Register the function as a benchmark
BENCHMARK(BM_DoUndoMove);

//static void BM_StringCreation(benchmark::State& state) {
//  for (auto _ : state)
//    std::string empty_string;
//}
//// Register the function as a benchmark
//BENCHMARK(BM_StringCreation);
//
//// Define another benchmark
//static void BM_StringCopy(benchmark::State& state) {
//  std::string x = "hello";
//  for (auto _ : state)
//    std::string copy(x);
//}
//BENCHMARK(BM_StringCopy);

BENCHMARK_MAIN();