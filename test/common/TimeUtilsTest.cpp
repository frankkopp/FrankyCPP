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


#include "common/Logging.h"
#include "common/stringutil.h"
#include "init.h"
#include "types/types.h"

#include <gtest/gtest.h>
using testing::Eq;

class TimeUtilsTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::warn);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TimeUtilsTest, printNanoseconds) {
  nanoseconds ns = nanoseconds{1888777999008800999};
  fprintln("{}", format(ns));
  EXPECT_EQ("59y:325d:20h:33m:19s:008.800.999ns", format(ns));

  ns = nanoseconds{1777999008800999};
  fprintln("{}", format(ns));
  EXPECT_EQ("20d:13h:53m:19s:008.800.999ns", format(ns));

  ns = nanoseconds{1999008800999};
  fprintln("{}", format(ns));
  EXPECT_EQ("33m:19s:008.800.999ns", format(ns));

  ns = nanoseconds{1000000999};
  fprintln("{}", format(ns));
  EXPECT_EQ("1s:000.000.999ns", format(ns));

  ns = nanoseconds{1000099};
  fprintln("{}", format(ns));
  EXPECT_EQ("1.000.099ns", format(ns));

  ns = nanoseconds{10000};
  fprintln("{}", format(ns));
  EXPECT_EQ("10.000ns", format(ns));

  ns = nanoseconds{100};
  fprintln("{}", format(ns));
  EXPECT_EQ("100ns", format(ns));
}

TEST_F(TimeUtilsTest, printMilliseconds) {
  milliseconds ms = milliseconds{49777999008800999};
  fprintln("{}", format(ms));
  EXPECT_EQ("275y:128d:10h:45m:32s:628.740.032ns", format(ms));

  ms = milliseconds{1999008800999};
  fprintln("{}", format(ms));
  EXPECT_EQ("63y:141d:16h:13m:20s:999.000.000ns", format(ms));

  ms = milliseconds{1000000999};
  fprintln("{}", format(ms));
  EXPECT_EQ("11d:13h:46m:40s:999.000.000ns", format(ms));

  ms = milliseconds{1000099};
  fprintln("{}", format(ms));
  EXPECT_EQ("16m:40s:099.000.000ns", format(ms));

  ms = milliseconds{10000};
  fprintln("{}", format(ms));
  EXPECT_EQ("10s:000.000.000ns", format(ms));

  ms = milliseconds{100};
  fprintln("{}", format(ms));
  EXPECT_EQ("100.000.000ns", format(ms));
}