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

#ifndef FRANKYCPP_SEARCHTREESIZETEST_H
#define FRANKYCPP_SEARCHTREESIZETEST_H

#include <string>
#include <utility>
#include <vector>

#include <engine/Search.h>
#include <types/types.h>

namespace {
  struct SingleTest {
    std::string name;
    uint64_t nodes    = 0;
    uint64_t nps      = 0;
    uint64_t depth    = 0;
    uint64_t extra    = 0;
    uint64_t time     = 0;
    uint64_t special1 = 0;
    uint64_t special2 = 0;
    Move move         = MOVE_NONE;
    Value value       = VALUE_NONE;
    std::string pv;
  };

  struct Result {
    std::string fen;
    std::vector<SingleTest> tests{};
    explicit Result(std::string _fen) : fen(std::move(_fen)){};
  };

  struct TestSums {
    uint64_t sumCounter{};
    uint64_t sumNodes{};
    uint64_t sumNps{};
    uint64_t sumDepth{};
    uint64_t sumExtra{};
    uint64_t sumTime{};
    uint64_t special1{};
    uint64_t special2{};
  };
}// namespace

class SearchTreeSizeTest {

  int depth;
  MilliSec movetime;
  std::vector<std::string> fens;
  std::vector<Result> results{};

  /* special is used to collect a dedicated stat */
  const uint64_t* ptrToSpecial1 = nullptr;
  const uint64_t* ptrToSpecial2 = nullptr;

public:
  SearchTreeSizeTest(int depth, const MilliSec& movetime, std::vector<std::string> fenVector)
      : depth(depth), movetime(movetime), fens(std::move(fenVector)) {}

  void start();

private:
  Result featureMeasurements(int d, MilliSec mt, const std::string& fen);
  SingleTest measureTreeSize(Search& search, const Position& position, SearchLimits searchLimits, const std::string& featureName) const;
};


#endif//FRANKYCPP_SEARCHTREESIZETEST_H
