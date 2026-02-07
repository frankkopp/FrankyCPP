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

#ifndef FRANKYCPP_EPDPARSER_H
#define FRANKYCPP_EPDPARSER_H

//=============================================================================
// EpdParser.h - EPD Test File Parser
//=============================================================================
//
// Parses EPD (Extended Position Description) test files into Test objects.
//
// Supported EPD Operations:
// - bm (best move): Engine's move must match one from the set
// - am (avoid move): Engine's move must not match any from the set
// - dm (direct mate): Engine must find mate in N moves
//
// EPD Format:
//   <FEN> <operation> <operand> ; [ id "<ID>" ; ]
//
// Examples:
//   r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 2 3 bm Ng5 Qh5+ ; id "Test1" ;
//   8/8/8/8/8/8/6K1/6Qr b - - 0 1 dm 1 ; id "Mate in 1" ;
//   rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 am a3 h3 ; id "Avoid flank" ;
//
// Line Processing:
// - Comments starting with '#' are removed
// - Trailing comments after ';' are preserved
// - Empty lines are skipped
// - Invalid lines are logged and skipped
//
// Move Validation:
// - SAN moves are validated against the position
// - Annotations (!, ?, !!, ??, etc.) are stripped
// - Invalid moves cause the line to be skipped
//
// Usage:
//   std::vector<Test> tests = EpdParser::parseFile("path/to/suite.epd");
//   // or
//   std::optional<Test> test = EpdParser::parseOneLine(epdLine);
//
//=============================================================================

#include "EdpTest.h"
#include "chesscore/Position.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Parses EPD test files into Test objects.
/// Stateless utility class with static methods.
class EpdParser {
  /// Cleans up an EPD line by removing comments and trimming whitespace.
  /// Leading comments (starting with '#') remove the entire line.
  /// Trailing comments (after content, starting with '#') are removed up to ';'.
  /// @param line Line to clean
  /// @return Cleaned line (empty string if line is pure comment)
  [[nodiscard]] static std::string cleanUpLine(std::string_view line);

  /// Parses test type from operation string.
  /// @param typeStr Operation string ("bm", "am", or "dm")
  /// @return TestType if valid, std::nullopt otherwise
  [[nodiscard]] static std::optional<TestType> parseTestType(std::string_view typeStr);

  /// Parses SAN moves for BM/AM tests.
  /// Validates moves against the position and strips annotations.
  /// @param movesStr Space-separated SAN moves (may include annotations like !, ?)
  /// @param pos Position to validate moves against
  /// @return MoveList of valid moves (empty if none valid)
  [[nodiscard]] static MoveList parseMoves(std::string_view movesStr, const Position& pos);

  /// Parses mate depth for DM tests.
  /// @param depthStr String representation of mate depth (e.g., "1", "3")
  /// @return Depth value if valid positive integer, std::nullopt otherwise
  [[nodiscard]] static std::optional<Depth> parseMateDepth(std::string_view depthStr);

public:
  /// Parses an entire EPD file into a vector of EpdTest objects.
  /// Invalid or unparseable lines are skipped with warnings logged.
  /// @param filePath Path to EPD file
  /// @return Vector of valid EpdTest objects (may be empty if file invalid or all lines skipped)
  [[nodiscard]] static std::vector<EpdTest> parseFile(std::string_view filePath);

  /// Parses a single EPD line into an EpdTest object.
  /// @param line EPD line to parse (may contain comments)
  /// @return EpdTest object if valid, std::nullopt if invalid/empty/comment
  [[nodiscard]] static std::optional<EpdTest> parseOneLine(std::string_view line);
};

#endif // FRANKYCPP_EPDPARSER_H
