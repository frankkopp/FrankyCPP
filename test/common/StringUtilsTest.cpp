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

class StringUtilsTest : public ::testing::Test {
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

TEST_F(StringUtilsTest, splitFastTest) {
  const std::string line{"1. f4 d5 2. Nf3 Nf6 3. e3 g6 4. b3 Bg7 5. Bb2 O-O 6. Be2 c5 7. O-O Nc6 8. Ne5 Qc7 1/2-1/2"};
  const std::string_view lineView{line};

  //  fprintln("String View:");
  std::vector<std::string_view> list1{};
  splitFast(lineView, list1, " ");
  //  fprintln("Elements: {}", list1.size());
  //  for(const auto& e : list1) {
  //    fprintln("{}", e);
  //  }
  EXPECT_EQ(25, list1.size());

  //  fprintln("Strings:");
  std::vector<std::string> list2{};
  splitFast(line, list2, " ");
  //  fprintln("Elements: {}", list2.size());
  //  for(const auto& e : list2) {
  //    fprintln("{}", e);
  //  }
  EXPECT_EQ(25, list2.size());
}

TEST_F(StringUtilsTest, trimFastTest) {
  const std::string line{" \t This is a text. This is a text. This is a text. This is a text.\t  \r\n"};
  const std::string_view lineView{line};

  auto trimmedString = trimFast(line);
  EXPECT_EQ("This is a text. This is a text. This is a text. This is a text.", trimmedString);

  auto trimmedView = trimFast(lineView);
  EXPECT_EQ("This is a text. This is a text. This is a text. This is a text.", trimmedView);
}

TEST_F(StringUtilsTest, removeTrailingCommentTest) {
  const std::string line{"This is a text. This is a text. This is a text. ; and this is the comment"};
  const std::string_view lineView{line};

  auto trimmedString = removeTrailingComments(line, ";");
  EXPECT_EQ("This is a text. This is a text. This is a text. ", trimmedString);

  auto trimmedView = removeTrailingComments(lineView, ";");
  EXPECT_EQ("This is a text. This is a text. This is a text. ", trimmedView);
}
