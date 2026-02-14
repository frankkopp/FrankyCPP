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

#include "common/Logging.h"
#include "common/stringutil.h"
#include "init.h"

#include <gtest/gtest.h>

using testing::Eq;

class StringUtilsTest : public testing::Test {
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

  const auto trimmedString = trimFast(line);
  EXPECT_EQ("This is a text. This is a text. This is a text. This is a text.", trimmedString);

  const auto trimmedView = trimFast(lineView);
  EXPECT_EQ("This is a text. This is a text. This is a text. This is a text.", trimmedView);
}

TEST_F(StringUtilsTest, removeTrailingCommentTest) {
  const std::string line{"This is a text. This is a text. This is a text. ; and this is the comment"};
  const std::string_view lineView{line};

  const auto trimmedString = removeTrailingComments(line, ";");
  EXPECT_EQ("This is a text. This is a text. This is a text. ", trimmedString);

  const auto trimmedView = removeTrailingComments(lineView, ";");
  EXPECT_EQ("This is a text. This is a text. This is a text. ", trimmedView);
}

//=============================================================================
// parseInt Tests (throwing variant)
//=============================================================================

TEST_F(StringUtilsTest, parseIntValidInput) {
  EXPECT_EQ(42, parseInt("42"));
  EXPECT_EQ(-17, parseInt("-17"));
  EXPECT_EQ(0, parseInt("0"));
  EXPECT_EQ(2147483647, parseInt("2147483647"));
  EXPECT_EQ(-2147483648, parseInt("-2147483648"));
}

TEST_F(StringUtilsTest, parseIntWithWhitespace) {
  // std::stoi allows leading whitespace
  EXPECT_EQ(42, parseInt("  42"));
  EXPECT_EQ(42, parseInt("\t42"));
}

TEST_F(StringUtilsTest, parseIntInvalidInput) {
  EXPECT_THROW((void)parseInt(""), std::invalid_argument);
  EXPECT_THROW((void)parseInt("abc"), std::invalid_argument);
  EXPECT_THROW((void)parseInt("99999999999999999999"), std::out_of_range);
}

TEST_F(StringUtilsTest, parseIntPartialMatch) {
  // std::stoi parses until invalid character - this is expected behavior
  EXPECT_EQ(12, parseInt("12.34"));  // stops at decimal point
  EXPECT_EQ(42, parseInt("42abc"));  // stops at letter
}

//=============================================================================
// parseDouble Tests (throwing variant)
//=============================================================================

TEST_F(StringUtilsTest, parseDoubleValidInput) {
  EXPECT_DOUBLE_EQ(3.14, parseDouble("3.14"));
  EXPECT_DOUBLE_EQ(-2.5, parseDouble("-2.5"));
  EXPECT_DOUBLE_EQ(0.0, parseDouble("0"));
  EXPECT_DOUBLE_EQ(42.0, parseDouble("42"));
  EXPECT_DOUBLE_EQ(1e10, parseDouble("1e10"));
}

TEST_F(StringUtilsTest, parseDoubleInvalidInput) {
  EXPECT_THROW((void)parseDouble(""), std::invalid_argument);
  EXPECT_THROW((void)parseDouble("abc"), std::invalid_argument);
}

//=============================================================================
// parseBool Tests (lenient, never throws)
//=============================================================================

TEST_F(StringUtilsTest, parseBoolTrueValues) {
  // All recognized true values
  EXPECT_TRUE(parseBool("true"));
  EXPECT_TRUE(parseBool("TRUE"));
  EXPECT_TRUE(parseBool("True"));
  EXPECT_TRUE(parseBool("1"));
  EXPECT_TRUE(parseBool("on"));
  EXPECT_TRUE(parseBool("ON"));
  EXPECT_TRUE(parseBool("yes"));
  EXPECT_TRUE(parseBool("YES"));
  EXPECT_TRUE(parseBool("+"));
}

TEST_F(StringUtilsTest, parseBoolFalseValues) {
  // Everything else is false
  EXPECT_FALSE(parseBool("false"));
  EXPECT_FALSE(parseBool("FALSE"));
  EXPECT_FALSE(parseBool("0"));
  EXPECT_FALSE(parseBool("off"));
  EXPECT_FALSE(parseBool("no"));
  EXPECT_FALSE(parseBool("-"));
  EXPECT_FALSE(parseBool(""));
  EXPECT_FALSE(parseBool("maybe"));
  EXPECT_FALSE(parseBool("2"));
  EXPECT_FALSE(parseBool("truthy"));
}

//=============================================================================
// parseString Tests (identity function)
//=============================================================================

TEST_F(StringUtilsTest, parseStringIdentity) {
  EXPECT_EQ("hello", parseString("hello"));
  EXPECT_EQ("", parseString(""));
  EXPECT_EQ("  spaces  ", parseString("  spaces  "));
}

//=============================================================================
// parseIntOr Tests (safe variant with logging)
//=============================================================================

TEST_F(StringUtilsTest, parseIntOrValidInput) {
  EXPECT_EQ(42, parseIntOr("42"));
  EXPECT_EQ(-17, parseIntOr("-17", 999));
  EXPECT_EQ(0, parseIntOr("0", 999));
}

TEST_F(StringUtilsTest, parseIntOrInvalidInput) {
  // Returns default on invalid input (and logs warning)
  EXPECT_EQ(0, parseIntOr("abc"));           // default default is 0
  EXPECT_EQ(999, parseIntOr("abc", 999));    // custom default
  EXPECT_EQ(-1, parseIntOr("", -1));         // empty string
  EXPECT_EQ(42, parseIntOr("not_a_number", 42));
}

//=============================================================================
// parseDoubleOr Tests (safe variant with logging)
//=============================================================================

TEST_F(StringUtilsTest, parseDoubleOrValidInput) {
  EXPECT_DOUBLE_EQ(3.14, parseDoubleOr("3.14"));
  EXPECT_DOUBLE_EQ(-2.5, parseDoubleOr("-2.5", 999.0));
  EXPECT_DOUBLE_EQ(0.0, parseDoubleOr("0", 999.0));
}

TEST_F(StringUtilsTest, parseDoubleOrInvalidInput) {
  // Returns default on invalid input (and logs warning)
  EXPECT_DOUBLE_EQ(0.0, parseDoubleOr("abc"));
  EXPECT_DOUBLE_EQ(1.5, parseDoubleOr("abc", 1.5));
  EXPECT_DOUBLE_EQ(-1.0, parseDoubleOr("", -1.0));
}

//=============================================================================
// tryParseInt Tests (optional variant)
//=============================================================================

TEST_F(StringUtilsTest, tryParseIntValidInput) {
  auto result = tryParseInt("42");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(42, result.value());

  result = tryParseInt("-17");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(-17, result.value());

  result = tryParseInt("0");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(0, result.value());
}

TEST_F(StringUtilsTest, tryParseIntInvalidInput) {
  EXPECT_FALSE(tryParseInt("abc").has_value());
  EXPECT_FALSE(tryParseInt("").has_value());
  EXPECT_FALSE(tryParseInt("99999999999999999999").has_value());
}

TEST_F(StringUtilsTest, tryParseIntDistinguishZeroFromInvalid) {
  // This is the key use case for optional variant
  const auto validZero = tryParseInt("0");
  const auto invalid = tryParseInt("invalid");

  EXPECT_TRUE(validZero.has_value());
  EXPECT_EQ(0, validZero.value());
  EXPECT_FALSE(invalid.has_value());
}

//=============================================================================
// tryParseDouble Tests (optional variant)
//=============================================================================

TEST_F(StringUtilsTest, tryParseDoubleValidInput) {
  auto result = tryParseDouble("3.14");
  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(3.14, result.value());

  result = tryParseDouble("0.0");
  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(0.0, result.value());
}

TEST_F(StringUtilsTest, tryParseDoubleInvalidInput) {
  EXPECT_FALSE(tryParseDouble("abc").has_value());
  EXPECT_FALSE(tryParseDouble("").has_value());
}

//=============================================================================
// truncate Tests (string truncation with ".." suffix)
//=============================================================================

TEST_F(StringUtilsTest, truncateStringFitsWidth) {
  // String fits within width - no truncation
  EXPECT_EQ("hello", truncate(std::string("hello"), 10));
  EXPECT_EQ("hello", truncate(std::string("hello"), 5));
  EXPECT_EQ("", truncate(std::string(""), 10));
}

TEST_F(StringUtilsTest, truncateStringExceedsWidth) {
  // String exceeds width - truncate with ".."
  EXPECT_EQ("he..", truncate(std::string("hello"), 4));
  EXPECT_EQ("hello w..", truncate(std::string("hello world"), 9));
  EXPECT_EQ("h..", truncate(std::string("hello"), 3));
}

TEST_F(StringUtilsTest, truncateEdgeCases) {
  // Width <= 2: just return ".." for strings that need truncation
  EXPECT_EQ("..", truncate(std::string("hello"), 2));
  EXPECT_EQ("..", truncate(std::string("hello"), 1));

  // Width <= 0: return empty
  EXPECT_EQ("", truncate(std::string("hello"), 0));
  EXPECT_EQ("", truncate(std::string("hello"), -5));

  // Short strings that fit even in tiny widths
  EXPECT_EQ("ab", truncate(std::string("ab"), 2));
  EXPECT_EQ("a", truncate(std::string("a"), 1));
}

TEST_F(StringUtilsTest, truncateStringViewFitsWidth) {
  // string_view version - fits within width
  const std::string_view sv = "hello world";
  EXPECT_EQ("hello world", truncate(sv, 20));
  EXPECT_EQ("hello world", truncate(sv, 11));
}

TEST_F(StringUtilsTest, truncateStringViewExceedsWidth) {
  // string_view version - exceeds width
  const std::string_view sv = "hello world";
  EXPECT_EQ("hello wo..", truncate(sv, 10));
  EXPECT_EQ("hel..", truncate(sv, 5));
  EXPECT_EQ("h..", truncate(sv, 3));
}

TEST_F(StringUtilsTest, truncateStringViewEdgeCases) {
  const std::string_view sv = "hello";
  EXPECT_EQ("..", truncate(sv, 2));
  EXPECT_EQ("", truncate(sv, 0));

  // Empty string_view
  const std::string_view empty = "";
  EXPECT_EQ("", truncate(empty, 10));
  EXPECT_EQ("", truncate(empty, 0));
}
