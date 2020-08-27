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

#include "types/types.h"
#include "chesscore/Position.h"
#include "engine/SearchLimits.h"

#include <gtest/gtest.h>
using testing::Eq;

#include <iostream>
#include <type_traits>

struct TestStructure {
  // no time control
  bool infinite = false;
  bool ponder   = false;
  int mate      = 0;

  // extra limits
  int depth      = 0;
  uint64_t nodes = 0;
//  MoveList moves{};

  //  time control;
  bool timeControl   = false;
  milliseconds whiteTime{0};
  milliseconds blackTime{0};
  milliseconds whiteInc{0};
  milliseconds blackInc{0};
  milliseconds moveTime{0};
};

TEST(Playground, format) {
  fprintln("Position:     {}", std::is_trivially_copyable<Position>::value);
  fprintln("SearchLimits: {}", std::is_trivially_copyable<SearchLimits>::value);
  fprintln("SearchLimits: {}", std::is_trivially_copyable<TestStructure>::value);
}
