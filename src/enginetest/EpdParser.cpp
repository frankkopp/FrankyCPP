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

#include "EpdParser.h"

#include "chesscore/MoveGenerator.h"
#include "common/Logging.h"
#include "common/stringutil.h"

#include <boost/algorithm/string.hpp>
#include <fstream>
#include <regex>
#include <sstream>

std::vector<EpdTest> EpdParser::parseFile(std::string_view filePath) {
  std::vector<EpdTest> tests;
  // Convert string_view to string for ifstream (needs null-terminated path)
  std::ifstream file{std::string(filePath)};

  if (!file.is_open()) {
    LOG__ERROR(Logger::get().TSUITE_LOG, "Could not open file: {}", filePath);
    return tests;
  }

  std::string line;
  while (std::getline(file, line)) {
    if (auto test = parseOneLine(line); test.has_value()) {
      tests.push_back(std::move(test.value()));
    }
  }

  file.close();
  return tests;
}

std::optional<EpdTest> EpdParser::parseOneLine(std::string_view line) {
  // Clean up the line
  std::string cleanedLine = cleanUpLine(line);

  LOG__DEBUG(Logger::get().TSUITE_LOG, "EPD: {}", cleanedLine);

  // Skip empty lines
  if (cleanedLine.empty()) {
    return std::nullopt;
  }

  // Parse EPD format: <FEN> <operation> <operand> ; [ id "<ID>" ; ]
  // Allow optional whitespace before semicolons for flexibility
  std::regex regexPattern(R"(^\s*(.*) (bm|dm|am) (.*?)\s*;(?:.*id\s*\"(.*?)\"\s*;)?.*$)");
  std::smatch matcher;

  if (!std::regex_match(cleanedLine, matcher, regexPattern)) {
    LOG__WARN(Logger::get().TSUITE_LOG, "No EPD match found in {}", cleanedLine);
    return std::nullopt;
  }

  // Extract parts
  std::string fen    = matcher.str(1);
  std::string typeStr = matcher.str(2);
  std::string result = matcher.str(3);
  std::string id     = matcher.str(4).empty() ? "no ID" : matcher.str(4);

  LOG__DEBUG(Logger::get().TSUITE_LOG, "Fen: {}    Type: {}    Result: {}    ID: {}",
             fen, typeStr, result, id);

  // Validate position
  Position pos;
  try {
    pos = Position(fen);
  } catch (std::invalid_argument& e) {
    LOG__WARN(Logger::get().TSUITE_LOG, "Invalid fen {} could not create position from: {}",
              e.what(), cleanedLine);
    return std::nullopt;
  }

  // Parse test type
  auto testType = parseTestType(typeStr);
  if (!testType.has_value()) {
    LOG__WARN(Logger::get().TSUITE_LOG, "Invalid TestType {}", typeStr);
    return std::nullopt;
  }

  // Build the test using Builder pattern
  EpdTest::Builder builder;
  builder.setId(std::move(id))
         .setFen(std::move(fen))
         .setLine(std::string(line))
         .setType(testType.value());

  // Parse test-specific data
  if (testType.value() == TestType::BM || testType.value() == TestType::AM) {
    // Parse moves
    MoveList moves = parseMoves(result, pos);
    if (moves.empty()) {
      LOG__WARN(Logger::get().TSUITE_LOG, "Result moves from EPD {} are invalid on this position {}",
                result, pos.strFen());
      return std::nullopt;
    }
    builder.setExpectedMove(moves[0]); // First move for display - BEFORE move!
    builder.setTargetMoves(std::move(moves));
  }
  else { // TestType::DM
    auto depth = parseMateDepth(result);
    if (!depth.has_value()) {
      LOG__WARN(Logger::get().TSUITE_LOG, "Direct mate depth from EPD is invalid: {}", result);
      return std::nullopt;
    }
    builder.setMateDepth(depth.value());
  }

  // Construct optional from moved EpdTest
  return std::optional<EpdTest>(builder.build());
}

std::string EpdParser::cleanUpLine(const std::string_view line) {
  // Convert to string and trim
  auto cleaned = std::string(trimFast(line));

  // Remove whole-line comments (lines starting with #)
  const std::regex leadCommentTrim(R"(^\s*#.*$)");
  cleaned = std::regex_replace(cleaned, leadCommentTrim, "");

  // Remove trailing comments (# not followed by ;) and replace with ;
  const std::regex trailCommentTrim(R"(^(.*)#([^;]*)$)");
  cleaned = std::regex_replace(cleaned, trailCommentTrim, "$1;");

  return cleaned;
}

std::optional<TestType> EpdParser::parseTestType(const std::string_view typeStr) {
  if (typeStr == "dm") return TestType::DM;
  if (typeStr == "bm") return TestType::BM;
  if (typeStr == "am") return TestType::AM;
  return std::nullopt;
}

MoveList EpdParser::parseMoves(const std::string_view movesStr, const Position& pos) {
  MoveList moves;
  std::string movesString(movesStr);

  // Remove annotations (!, ?, !!, ??, etc.)
  boost::replace_all(movesString, "!", "");
  boost::replace_all(movesString, "?", "");

  // Split by spaces
  std::vector<std::string> moveStrings;
  boost::split(moveStrings, movesString, [](const char c) { return c == ' '; });

  // Validate each move
  MoveGenerator mg{};
  for (auto& moveStr : moveStrings) {
    boost::trim(moveStr);
    if (moveStr.empty()) continue;

    Move m = mg.getMoveFromSan(pos, moveStr);
    if (m.isValid()) {
      moves.push_back(m);
    }
  }

  return moves;
}

std::optional<Depth> EpdParser::parseMateDepth(const std::string_view depthStr) {
  int depth = 0;
  const std::string depthString(depthStr);
  std::istringstream iss(depthString);
  iss >> depth;

  if (depth > 0) {
    return static_cast<Depth>(depth);
  }
  return std::nullopt;
}
