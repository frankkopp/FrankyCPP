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

#include "common/pgn/PgnParser.h"
#include "common/Logging.h"
#include "common/stringutil.h"
#include "types/globals.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>

using namespace common::pgn;

namespace {

  // Lookup tables for character classification — avoids clang-tidy warnings
  // from locale-dependent C <cctype> functions and implicit char→int conversions.

  constexpr auto makeDigitTable() {
    std::array<bool, 256> t{};
    for (int c = '0'; c <= '9'; ++c) t[c] = true;
    return t;
  }

  // Valid PGN move-section characters (alnum + special PGN chars).
  // Characters NOT in this set are replaced with spaces during cleanup.
  constexpr auto makeValidMoveSectionCharTable() {
    std::array<bool, 256> t{};
    for (int c = '0'; c <= '9'; ++c) t[c] = true;
    for (int c = 'A'; c <= 'Z'; ++c) t[c] = true;
    for (int c = 'a'; c <= 'z'; ++c) t[c] = true;
    t['$'] = true;
    t['*'] = true;
    t['('] = true;
    t['{'] = true;
    t['<'] = true;
    t['/'] = true;
    t['-'] = true;
    t['='] = true;
    return t;
  }

  constexpr auto IS_DIGIT     = makeDigitTable();
  constexpr auto IS_VALID_PGN = makeValidMoveSectionCharTable();

  /// Safe character lookup — treats negative (non-ASCII) chars as false.
  constexpr bool isAscii(const char c) { return static_cast<unsigned char>(c) <= 127; }

} // anonymous namespace

// //////////////////////////////////////////////
// /// PUBLIC

void PgnParser::parseFile(const std::string& filePath, const GameCallback& gameCallback) {
  gamesProcessed = 0;
  gamesSkipped   = 0;

  std::vector<char> fileData;
  const auto lines = readFileLines(filePath, fileData);
  if (lines.empty()) {
    return;
  }

  processGames(lines, gameCallback);
}

std::vector<PgnGame> PgnParser::parseAll(const std::string& filePath) {
  std::vector<PgnGame> games;

  parseFile(filePath, [&games](PgnGame&& game) {
    games.push_back(std::move(game));
  });

  return games;
}

std::vector<PgnGame> PgnParser::parseFromLines(const std::vector<std::string_view>& lines) {
  std::vector<PgnGame> games;

  parseFromLines(lines, [&games](PgnGame&& game) {
    games.push_back(std::move(game));
  });

  return games;
}

void PgnParser::parseFromLines(const std::vector<std::string_view>& lines, const GameCallback& gameCallback) {
  gamesProcessed = 0;
  gamesSkipped   = 0;

  processGames(lines, gameCallback);
}

// //////////////////////////////////////////////
// /// PRIVATE

std::vector<std::string_view> PgnParser::readFileLines(
  const std::string& filePath,
  std::vector<char>& fileData) {

  std::vector<std::string_view> lines;

  if (!std::filesystem::exists(filePath)) {
    LOG__ERROR(Logger::get().BOOK_LOG, "PGN file '{}' not found.", filePath);
    return lines;
  }

  std::fstream file(filePath, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    LOG__ERROR(Logger::get().BOOK_LOG, "Could not open PGN file '{}'.", filePath);
    return lines;
  }

  // Fast read: load entire file into memory, then split by newlines.
  // Same approach as OpeningBook::readFile() — proven on multi-MB PGN files.
  file.seekg(0, std::ios::end);
  const auto dataSize = static_cast<std::size_t>(file.tellg());
  file.seekg(0, std::ios::beg);

  fileData.resize(dataSize);
  file.read(fileData.data(), static_cast<std::streamsize>(dataSize));
  file.close();

  lines.reserve(dataSize / 20); // rough estimate
  for (std::size_t i = 0, start = 0; i < dataSize; ++i) {
    if (fileData[i] == '\n' || i == dataSize - 1) {
      lines.emplace_back(fileData.data() + start, i - start);
      start = i + 1;
    }
  }

  LOG__DEBUG(Logger::get().BOOK_LOG, "PGN parser read {:L} lines from '{}'", lines.size(), filePath);
  return lines;
}

void PgnParser::processGames(const std::vector<std::string_view>& lines, const GameCallback& gameCallback) {
  // Identify game boundaries by looking for tag pair lines ([...]) after empty lines.
  // This replicates the game boundary detection from OpeningBook::readGamesPgn().
  const auto length = lines.size();
  size_t gameStart  = 0;
  bool lastEmpty    = true;

  for (size_t lineNumber = 0; lineNumber < length; ++lineNumber) {
    const auto trimmedLine = trimFast(lines[lineNumber]);

    if (trimmedLine.empty()) {
      lastEmpty = true;
      continue;
    }

    // A new game starts when a tag line appears after an empty line (or at file start)
    if (lastEmpty && trimmedLine[0] == '[') {
      // If this isn't the very first game start, process the previous game
      if (lineNumber > gameStart) {
        auto game = parseOneGame(lines, gameStart, lineNumber);
        if (game.has_value()) {
          gamesProcessed++;
          gameCallback(std::move(game.value()));
        }
        else {
          gamesSkipped++;
        }
      }
      gameStart = lineNumber;
    }
    lastEmpty = false;
  }

  // Process the final game (from gameStart to end of file)
  if (gameStart < length) {
    auto game = parseOneGame(lines, gameStart, length);
    if (game.has_value()) {
      gamesProcessed++;
      gameCallback(std::move(game.value()));
    }
    else {
      gamesSkipped++;
    }
  }

  LOG__DEBUG(Logger::get().BOOK_LOG, "PGN parser: {:L} games parsed, {:L} skipped",
             gamesProcessed, gamesSkipped);
}

std::optional<PgnGame> PgnParser::parseOneGame(
  const std::vector<std::string_view>& lines,
  const size_t gameStart,
  const size_t gameEnd) {

  PgnGame game;
  std::string moveLine;

  for (auto i = gameStart; i < gameEnd; ++i) {
    const auto lineView = trimFast(lines[i]);

    // Skip empty lines and %-escaped lines
    if (lineView.empty() || lineView[0] == '%') {
      continue;
    }

    // Parse tag pair lines (may contain multiple [Key "Value"] pairs per line)
    if (lineView[0] == '[') {
      // Strip ;-comments from end of tag line (e.g., [Result "1-0"];comment)
      const auto tagLine = removeTrailingComments(lineView, ";");
      std::string key;
      std::string value;
      size_t pos = 0;
      while (pos < tagLine.size()) {
        pos = parseTagPair(tagLine, pos, key, value);
        if (pos == std::string_view::npos) break;
        game.headers[key] = value;
      }
      continue;
    }

    // Accumulate move text lines (strip ;-comments first)
    moveLine.append(" ").append(removeTrailingComments(lineView, ";"));
  }

  // Parse result from header
  const auto resultIt = game.headers.find("Result");
  if (resultIt != game.headers.end()) {
    game.result = parseResultString(resultIt->second);
  }

  // Extract {}-comments from raw move text before cleanup strips them.
  // Each {}-block corresponds to the move immediately preceding it.
  // This provides engine scores, book markers, etc. for downstream consumers.
  if (!moveLine.empty()) {
    std::vector<std::string> comments;
    size_t searchPos = 0;
    while (searchPos < moveLine.size()) {
      const auto open = moveLine.find('{', searchPos);
      if (open == std::string::npos) break;
      const auto close = moveLine.find('}', open + 1);
      if (close == std::string::npos) break;
      comments.emplace_back(moveLine.substr(open + 1, close - open - 1));
      searchPos = close + 1;
    }
    game.moveComments = std::move(comments);
  }

  // Clean up move section and extract individual move strings
  if (!moveLine.empty()) {
    cleanUpMoveSection(moveLine);
  }
  if (!moveLine.empty()) {
    std::vector<std::string> moveStrings;
    splitFast(moveLine, moveStrings, " ");
    game.moves = std::move(moveStrings);
  }

  if (game.moves.empty()) {
    return std::nullopt;
  }

  return game;
}

size_t PgnParser::parseTagPair(
  const std::string_view line, const size_t startPos,
  std::string& key, std::string& value) {

  // Find the opening bracket
  const auto bracketPos = line.find('[', startPos);
  if (bracketPos == std::string_view::npos) {
    return std::string_view::npos;
  }

  // Find the space between key and quoted value
  const auto spacePos = line.find(' ', bracketPos + 1);
  if (spacePos == std::string_view::npos) {
    return std::string_view::npos;
  }

  // Extract key (between '[' and first space)
  key = std::string(line.substr(bracketPos + 1, spacePos - bracketPos - 1));

  // Find the opening quote for the value
  const auto firstQuote = line.find('"', spacePos);
  if (firstQuote == std::string_view::npos) {
    return std::string_view::npos;
  }

  // Find closing quote, handling escaped quotes (\") and backslashes (\\)
  value.clear();
  for (auto i = firstQuote + 1; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '\\' && i + 1 < line.size()) {
      // PGN escape: \\ → literal backslash, \" → literal quote
      const char next = line[i + 1];
      if (next == '"') {
        value += '"';
      }
      else if (next == '\\') {
        value += '\\';
      }
      else {
        // Unknown escape — keep both characters
        value += c;
        value += next;
      }
      ++i; // skip the escaped character
    }
    else if (c == '"') {
      // Closing quote found — find the ']' after it
      const auto closeBracket = line.find(']', i + 1);
      if (closeBracket != std::string_view::npos) {
        return closeBracket + 1; // position after ']'
      }
      return i + 1; // no ']' found, return position after closing quote
    }
    else {
      value += line[i];
    }
  }

  // No closing quote found
  return std::string_view::npos;
}

void PgnParser::cleanUpMoveSection(std::string& str) {
  // This is the logic from OpeningBook::cleanUpPgnMoveSection(),
  // using constexpr lookup tables instead of C locale <cctype> functions.
  const std::size_t length = str.length();
  if (length == 0) return;

  char lastChar = ' ';
  for (std::size_t a = 0; a < length;) {
    const auto uc = static_cast<unsigned char>(str[a]);

    // skip non-ascii and invalid characters (only valid PGN move-section chars pass)
    if (!isAscii(str[a]) || !IS_VALID_PGN[uc]) {
      str[a++] = ' ';
    }
    // nag annotation \$\d{1,3}
    else if (str[a] == '$') {
      str[a++] = ' ';
      while (a < length && IS_DIGIT[static_cast<unsigned char>(str[a])]) {
        str[a++] = ' ';
      }
    }
    // remove curly bracket comments '\{[^{}]*\}'
    else if (str[a] == '{') {
      while (a < length && str[a] != '}') {
        str[a++] = ' ';
      }
      if (a < length) str[a++] = ' ';
    }
    // remove tag bracket comments '\<[^<>]*\>'
    else if (str[a] == '<') {
      while (a < length && str[a] != '>') {
        str[a++] = ' ';
      }
      if (a < length) str[a++] = ' ';
    }
    // remove bracket comments '\([^()]*\)' — maybe recursive
    else if (str[a] == '(') {
      int open = 1;
      str[a++] = ' ';
      while (a < length && open > 0) {
        if (str[a] == ')')
          open--;
        else if (str[a] == '(')
          open++;
        str[a++] = ' ';
      }
    }
    // remove move numbering
    else if (IS_DIGIT[uc] && lastChar == ' ') {
      str[a++] = ' ';
      while (a < length && (IS_DIGIT[static_cast<unsigned char>(str[a])] || str[a] == '.')) {
        str[a++] = ' ';
      }
    }
    // valid — move forward
    else {
      a++;
    }
    lastChar = str[a - 1];
  }

  // remove result (1-0 0-1 1/2-1/2 *)
  bool resultFound   = false;
  std::size_t resPos = length;
  for (std::size_t a = length; a-- > 0;) {
    if (str[a] == ' ') {
      continue;
    }
    if (str[a] == '*') {
      str[a]      = ' ';
      resPos      = a;
      resultFound = true;
      break;
    }
    if (a >= 6 && str.substr(a - 6, 7) == " /2-1/2") {
      resPos      = a - 6;
      resultFound = true;
      break;
    }
    if (a >= 2 && (str.substr(a - 2, 3) == " -0" || str.substr(a - 2, 3) == " -1")) {
      resPos      = a - 2;
      resultFound = true;
      break;
    }
    // Stop scanning once we hit a non-space, non-result character
    break;
  }

  if (resultFound) {
    str.resize(resPos);
  }

  // Use the std::unique algorithm to remove consecutive spaces
  const auto newEnd = std::ranges::unique(str,
                                          [](const char aa, const char bb) { return aa == ' ' && bb == ' '; })
                        .begin();
  str.resize(std::distance(str.begin(), newEnd));

  // remove trailing and leading whitespace
  const auto trimmed = trimFast(std::string_view{str});
  str                = std::string{trimmed};
}
