// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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


#include "init.h"
#include "types/types.h"
#include "chesscore/Position.h"

#include <benchmark/benchmark.h>

#include <openingbook/OpeningBook.h>
#include <regex>

// TimingBench is meant as a collection of benchmarks for alternative implementations
// of certain code pieces to find the fastest alternative.
class TimingBench : public benchmark::Fixture {
public:
  void SetUp(const ::benchmark::State&) {
    init::init();
  }

  void TearDown(const ::benchmark::State&) {
  }

};

BENCHMARK_F(TimingBench, cleanUp)(benchmark::State& state) {
  std::string testString{"e4(d4) d5!!2.c4$50(Nf3?)e5 Nf3{Comment !}Nc6 Nc3 Nf6 Bc4 {another comment} Bc5 O-O O-O a1=Q  @@@æææ {unexpected characters are skipped}  <> {These symbols are reserved}  1/2-1/2  ; comment     "};
  double counter = 0;
  for (auto _ : state) {
    std::string test = removeTrailingComments(testString, ";");
    OpeningBook::cleanUpPgnMoveSection(test);
    counter++;
  }
  state.counters["Runs"] = counter;
  state.counters["RunRate"] = benchmark::Counter(counter, benchmark::Counter::kIsRate);
  state.counters["RunTime"] = benchmark::Counter(counter, benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}

BENCHMARK_F(TimingBench, BM_IllegalCharacter1)(benchmark::State& state) {
  const std::string fen{"r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1"};
  const std::regex illegalInFenPosition{R"([^1-8pPnNbBrRqQkK/]+)"};
  double counter{};
  for (auto _ : state) {
    if (!std::regex_search(fen, illegalInFenPosition)) {
      counter++;
    }
  }
}

BENCHMARK_F(TimingBench, BM_IllegalCharacter2)(benchmark::State& state) {
  const std::string fen{"r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1"};
  const std::string allowedChars{"12345678pPnNbBrRqQkK/"};
  double counter{};
  for (auto _ : state) {
    bool illegalFound = false;
    const auto l = fen.length();
    for (int i = 0; i < l; i++) {
      if (allowedChars.find(fen[i]) == std::string::npos) {
        illegalFound = true;
        break;
      }
    }
    if (!illegalFound) {
      counter++;
    }
  }
}
