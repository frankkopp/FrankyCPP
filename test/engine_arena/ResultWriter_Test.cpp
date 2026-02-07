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
// ResultWriter_Test.cpp - Unit Tests for ResultWriter JSON Output
//=============================================================================
//
// Tests for JSON output validity, especially string escaping for:
//   - Backslashes in Windows paths
//   - Quotes in engine names
//   - Special characters in FEN strings
//   - Control characters
//
//=============================================================================

#include "engine_arena/ResultWriter.h"
#include "engine_arena/ArenaResults.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using namespace arena;
using json = nlohmann::json;

class ResultWriterTest : public ::testing::Test {
protected:
  std::string testResultsDir;

  void SetUp() override {
    // Create a temporary directory for test results
    testResultsDir = "test_results_temp";
    std::filesystem::create_directories(testResultsDir);
  }

  void TearDown() override {
    // Clean up temporary directory
    std::filesystem::remove_all(testResultsDir);
  }

  // Helper to read file content
  std::string readFile(const std::string& path) {
    const std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  }

  // Helper to create a basic TestSuiteResult
  TestSuiteResult createBasicResult() {
    TestSuiteResult result;
    result.arenaVersion = "v1.1";
    result.timestamp = "2026-02-06T12:00:00Z";
    result.testSuiteName = "TestSuite";
    result.epdPath = "test/test.epd";
    result.engineName = "TestEngine";
    result.engineVersion = "1.0";
    result.enginePath = "path/to/engine.exe";
    result.totalTests = 1;
    result.passed = 1;
    result.failed = 0;
    result.skipped = 0;
    result.totalNodes = 1000;
    result.totalTimeMs = 100;
    return result;
  }
};

//=============================================================================
// JSON Validity Tests
//=============================================================================

TEST_F(ResultWriterTest, BasicResult_ProducesValidJson) {
  const ResultWriter writer(testResultsDir);
  const TestSuiteResult result = createBasicResult();

  const std::string filePath = writer.writeTestSuiteResult(result);

  // Read and parse as JSON - should not throw
  std::string content = readFile(filePath);
  ASSERT_NO_THROW({
    json j = json::parse(content);
  });
}

TEST_F(ResultWriterTest, WindowsPath_BackslashesEscaped) {
  const ResultWriter writer(testResultsDir);
  TestSuiteResult result = createBasicResult();

  // Windows-style path with backslashes
  result.enginePath = R"(D:\Games\Chess\Engines\FrankyCPP\engine.exe)";
  result.epdPath = R"(C:\Users\Frank\test\suite.epd)";

  const std::string filePath = writer.writeTestSuiteResult(result);

  // Should produce valid JSON
  std::string content = readFile(filePath);
  ASSERT_NO_THROW({
    json j = json::parse(content);
    // Verify the paths are correctly stored
    EXPECT_EQ(j["engine"]["path"], "D:\\Games\\Chess\\Engines\\FrankyCPP\\engine.exe");
    EXPECT_EQ(j["testSuite"]["epdPath"], "C:\\Users\\Frank\\test\\suite.epd");
  });
}

TEST_F(ResultWriterTest, QuotesInTestDetails_Escaped) {
  ResultWriter writer(testResultsDir);
  TestSuiteResult result = createBasicResult();

  // Test details with quotes (these don't affect filename)
  TestCaseDetail detail;
  detail.testId = "Test \"quoted\" ID";
  detail.fen = "8/8/8/8/8/8/8/8 w - - 0 1";
  detail.expected = "move \"best\"";
  detail.actual = "move \"actual\"";
  detail.passed = true;
  detail.nodes = 1000;
  detail.timeMs = 50;
  result.details.push_back(detail);

  std::string filePath = writer.writeTestSuiteResult(result);

  // Should produce valid JSON
  std::string content = readFile(filePath);
  ASSERT_NO_THROW({
    json j = json::parse(content);
    EXPECT_EQ(j["details"][0]["testId"], "Test \"quoted\" ID");
    EXPECT_EQ(j["details"][0]["expected"], "move \"best\"");
    EXPECT_EQ(j["details"][0]["actual"], "move \"actual\"");
  });
}

TEST_F(ResultWriterTest, FenWithSpecialChars_Escaped) {
  ResultWriter writer(testResultsDir);
  TestSuiteResult result = createBasicResult();

  // Add a test detail with a FEN containing special characters
  TestCaseDetail detail;
  detail.testId = "Test1";
  detail.fen = "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4";
  detail.expected = "Qxf7+";
  detail.actual = "Qxf7+";
  detail.passed = true;
  detail.nodes = 1000;
  detail.timeMs = 50;
  result.details.push_back(detail);

  std::string filePath = writer.writeTestSuiteResult(result);

  // Should produce valid JSON
  std::string content = readFile(filePath);
  ASSERT_NO_THROW({
    json j = json::parse(content);
    EXPECT_EQ(j["details"][0]["fen"], detail.fen);
  });
}

TEST_F(ResultWriterTest, ControlCharacters_Escaped) {
  ResultWriter writer(testResultsDir);
  TestSuiteResult result = createBasicResult();

  // Test details with control characters (tab, newline) - these don't affect filename
  TestCaseDetail detail;
  detail.testId = "Test\twith\ttabs\nand\nnewlines";
  detail.fen = "8/8/8/8/8/8/8/8 w - - 0 1";
  detail.expected = "a1a2\twith\ttab";
  detail.actual = "a1a2\nwith\nnewline";
  detail.passed = true;
  detail.nodes = 100;
  detail.timeMs = 10;
  result.details.push_back(detail);

  std::string filePath = writer.writeTestSuiteResult(result);

  // Should produce valid JSON (control chars escaped as \t, \n)
  std::string content = readFile(filePath);
  ASSERT_NO_THROW({
    json j = json::parse(content);
    // nlohmann::json will decode the escape sequences back
    EXPECT_EQ(j["details"][0]["testId"], "Test\twith\ttabs\nand\nnewlines");
  });
}

TEST_F(ResultWriterTest, MixedSpecialCharacters_AllEscaped) {
  ResultWriter writer(testResultsDir);
  TestSuiteResult result = createBasicResult();

  // Mix of backslashes, quotes, and control chars in paths and details
  // Note: enginePath with special chars is OK (doesn't affect filename)
  result.enginePath = "C:\\Program Files\\Chess\\engine.exe";

  // Test details with mixed special characters
  TestCaseDetail detail;
  detail.testId = "Test\\with\"quotes\tand\ttabs";
  detail.fen = "8/8/8/8/8/8/8/8 w - - 0 1";
  detail.expected = "e2e4";
  detail.actual = "e2e4";
  detail.passed = true;
  detail.nodes = 100;
  detail.timeMs = 10;
  result.details.push_back(detail);

  std::string filePath = writer.writeTestSuiteResult(result);

  // Should produce valid JSON
  std::string content = readFile(filePath);
  ASSERT_NO_THROW({
    json j = json::parse(content);
    EXPECT_EQ(j["engine"]["path"], "C:\\Program Files\\Chess\\engine.exe");
    EXPECT_EQ(j["details"][0]["testId"], "Test\\with\"quotes\tand\ttabs");
  });
}

TEST_F(ResultWriterTest, EmptyStrings_HandledCorrectly) {
  const ResultWriter writer(testResultsDir);
  TestSuiteResult result = createBasicResult();

  // Empty version string
  result.engineVersion = "";

  const std::string filePath = writer.writeTestSuiteResult(result);

  // Should produce valid JSON
  std::string content = readFile(filePath);
  ASSERT_NO_THROW({
    json j = json::parse(content);
    EXPECT_EQ(j["engine"]["version"], "");
  });
}

TEST_F(ResultWriterTest, UnicodeCharacters_HandledCorrectly) {
  ResultWriter writer(testResultsDir);
  TestSuiteResult result = createBasicResult();

  // Unicode in test details (not in engine name/version which affect filename)
  TestCaseDetail detail;
  detail.testId = "Тест_001";  // Russian "Test"
  detail.fen = "8/8/8/8/8/8/8/8 w - - 0 1";
  detail.expected = "Шах";  // Russian "Check"
  detail.actual = "Шах";
  detail.passed = true;
  detail.nodes = 100;
  detail.timeMs = 10;
  result.details.push_back(detail);

  std::string filePath = writer.writeTestSuiteResult(result);

  // Should produce valid JSON
  std::string content = readFile(filePath);
  ASSERT_NO_THROW({
    json j = json::parse(content);
  });
}
