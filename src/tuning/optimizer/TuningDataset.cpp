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

#include "tuning/optimizer/TuningDataset.h"

#include "chesscore/Position.h"
#include "common/Logging.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace tuning {

  void TuningDataset::loadFromFile(const std::string& path, const std::size_t maxEntries) {
    // Reset state
    entries.clear();
    loadStats = {};

    // Validate file exists and is readable
    if (!std::filesystem::exists(path)) {
      throw std::runtime_error("Dataset file not found: " + path);
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
      throw std::runtime_error("Cannot open dataset file: " + path);
    }

    // Estimate entry count for reserve (average ~60 chars per line)
    const auto fileSize       = std::filesystem::file_size(path);
    const auto estimatedLines = fileSize / 60;
    const auto reserveCount   = maxEntries > 0 ? std::min(estimatedLines, maxEntries) : estimatedLines;
    entries.reserve(reserveCount);

    const auto startTime = steady_clock::now();

    // Single reusable Position for FEN validation — avoids constructing/destroying
    // a ~33KB object on every line (Position has historyState[1024]).
    chess::Position validator;

    std::string line;
    while (std::getline(ifs, line)) {
      // Stop if we've reached the entry limit
      if (maxEntries > 0 && entries.size() >= maxEntries) break;

      // Skip empty lines
      if (line.empty() || std::ranges::all_of(line, isspace)) {
        loadStats.skippedEmpty++;
        continue;
      }

      // Skip comment lines
      if (line[0] == '#' || (line.size() >= 2 && line[0] == '/' && line[1] == '/')) {
        loadStats.skippedComment++;
        continue;
      }

      loadStats.totalLines++;

      // Progress reporting every 250K lines
      if (loadStats.totalLines % 250'000 == 0) {
        std::cout << "  Loading... " << loadStats.totalLines << " lines\r" << std::flush;
      }

      // Try FrankyCPP format first (most common in our pipeline): <FEN> [<result>]
      TuningEntry entry;
      if (parseFrankyCppFormat(line, entry, validator)) {
        entries.push_back(entry);
        loadStats.parsedOk++;
        continue;
      }

      // Try EPD c9 format: <FEN-fields> c9 "<result>";
      if (parseEpdFormat(line, entry, validator)) {
        entries.push_back(entry);
        loadStats.parsedOk++;
        continue;
      }

      // Neither format matched
      loadStats.skippedMalformed++;
      if (loadStats.skippedMalformed <= 5) {
        LOG__WARN(common::Logger::get().TUNING_LOG, "Skipping malformed line {}: {}",
                  loadStats.totalLines,
                  line.substr(0, std::min(line.size(), static_cast<std::size_t>(80))));
      }
    }

    const auto endTime   = steady_clock::now();
    const auto elapsedMs = std::chrono::duration_cast<milliseconds>(endTime - startTime).count();

    // Clear the in-line progress indicator
    if (loadStats.totalLines >= 250'000) {
      std::cout << std::string(50, ' ') << "\r" << std::flush;
    }

    // Shrink to fit after loading
    entries.shrink_to_fit();

    // Log summary
    LOG__INFO(common::Logger::get().TUNING_LOG, "Dataset loaded from: {}", path);
    LOG__INFO(common::Logger::get().TUNING_LOG, "  Entries:     {:>10}", entries.size());
    LOG__INFO(common::Logger::get().TUNING_LOG, "  Parsed OK:   {:>10}", loadStats.parsedOk);
    LOG__INFO(common::Logger::get().TUNING_LOG, "  Skipped:     {:>10} (empty: {}, comment: {}, malformed: {})",
              loadStats.skippedEmpty + loadStats.skippedComment + loadStats.skippedMalformed,
              loadStats.skippedEmpty, loadStats.skippedComment, loadStats.skippedMalformed);
    LOG__INFO(common::Logger::get().TUNING_LOG, "  Elapsed:     {:>10} ms", elapsedMs);

    std::cout << "Dataset loaded: " << entries.size() << " positions from " << path
              << " (" << elapsedMs << " ms)\n";
  }

  std::pair<TuningDataset, TuningDataset> TuningDataset::split(const float trainFraction) const {
    // Clamp fraction to valid range
    const float fraction = std::clamp(trainFraction, 0.0F, 1.0F);
    const auto trainSize = static_cast<std::size_t>(
      std::round(static_cast<float>(entries.size()) * fraction));

    TuningDataset train;
    TuningDataset test;
    train.entries.reserve(trainSize);
    test.entries.reserve(entries.size() - trainSize);

    for (std::size_t i = 0; i < entries.size(); ++i) {
      if (i < trainSize) {
        train.entries.push_back(entries[i]);
      }
      else {
        test.entries.push_back(entries[i]);
      }
    }

    return {std::move(train), std::move(test)};
  }

  // =========================================================================
  // Private parsing methods
  // =========================================================================

  bool TuningDataset::parseFrankyCppFormat(const std::string& line, TuningEntry& entry,
                                            chess::Position& validator) {
    // Format: <FEN> [<result>]
    // FEN has 6 space-separated fields: pieces, color, castling, en-passant, half-move, full-move
    // Result is enclosed in brackets at the end: [1.0], [0.5], [0.0]

    // Find the result tag: last occurrence of '[' ... ']'
    const auto bracketOpen  = line.rfind('[');
    const auto bracketClose = line.rfind(']');

    if (bracketOpen == std::string::npos || bracketClose == std::string::npos
        || bracketClose <= bracketOpen) {
      return false;
    }

    // Extract result string between brackets
    const std::string resultStr = line.substr(bracketOpen + 1, bracketClose - bracketOpen - 1);
    float result                = 0.0F;
    if (!parseResult(resultStr, result)) {
      return false;
    }

    // Extract FEN: everything before the bracket, trimmed
    std::string fen = line.substr(0, bracketOpen);
    // Trim trailing whitespace
    while (!fen.empty() && std::isspace(static_cast<unsigned char>(fen.back()))) {
      fen.pop_back();
    }

    if (fen.empty()) {
      return false;
    }

    // Validate FEN has at least 4 fields (pieces, color, castling, en-passant)
    // Full FEN has 6 fields, but some datasets omit half-move/full-move counters
    {
      int spaceCount = 0;
      for (const char c : fen) {
        if (c == ' ') spaceCount++;
      }
      if (spaceCount < 3) {
        return false; // Not enough FEN fields
      }
    }

    // Validate FEN via reusable Position (throws on invalid FEN).
    // We store only the FEN string — Position is reconstructed during MSE computation.
    try {
      validator.setFromFen(fen);
      entry.fen    = std::move(fen);
      entry.result = result;
      return true;
      // ReSharper disable once CppDFAUnreachableCode
    } catch (const std::exception&) { // defensive: setFromFen may throw on malformed FEN
      return false;
    }
  }

  bool TuningDataset::parseEpdFormat(const std::string& line, TuningEntry& entry,
                                      chess::Position& validator) {
    // Format: <FEN-fields> c9 "<PGN-result>";
    // EPD has 4 FEN fields (no half-move/full-move counters) followed by operations.
    // The c9 operation contains the game result.

    // Find the c9 tag
    const auto c9Pos = line.find("c9 ");
    if (c9Pos == std::string::npos) {
      return false;
    }

    // Extract the result string after c9
    // Look for quoted result: c9 "1-0"; or c9 "1/2-1/2"; or c9 "0-1";
    const auto quoteStart = line.find('"', c9Pos);
    const auto quoteEnd   = line.find('"', quoteStart + 1);
    if (quoteStart == std::string::npos || quoteEnd == std::string::npos) {
      return false;
    }

    const std::string resultStr = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    float result                = 0.0F;
    if (!parseResult(resultStr, result)) {
      return false;
    }

    // Extract FEN fields: everything before the c9 tag, trimmed
    std::string fenPart = line.substr(0, c9Pos);
    // Trim trailing whitespace only (the '-' may be the en-passant field)
    while (!fenPart.empty() && std::isspace(static_cast<unsigned char>(fenPart.back()))) {
      fenPart.pop_back();
    }

    if (fenPart.empty()) {
      return false;
    }

    // EPD has only 4 FEN fields (pieces, color, castling, en-passant).
    // Position constructor expects a full FEN, so append default counters.
    // Count spaces to determine if we need to add counters.
    int spaceCount = 0;
    for (const char c : fenPart) {
      if (c == ' ') spaceCount++;
    }

    std::string fen = fenPart;
    if (spaceCount == 3) {
      // 4 fields only — append default half-move and full-move counters
      fen += " 0 1";
    }
    // If spaceCount >= 5, it already has all 6 fields

    // Validate FEN via reusable Position (throws on invalid FEN).
    // We store only the FEN string — Position is reconstructed during MSE computation.
    try {
      validator.setFromFen(fen);
      entry.fen    = std::move(fen);
      entry.result = result;
      return true;
      // ReSharper disable once CppDFAUnreachableCode
    } catch (const std::exception&) { // defensive: setFromFen may throw on malformed FEN
      return false;
    }
  }

  bool TuningDataset::parseResult(const std::string& resultStr, float& result) {
    // FrankyCPP numeric format
    if (resultStr == "1.0") {
      result = 1.0F;
      return true;
    }
    if (resultStr == "0.5") {
      result = 0.5F;
      return true;
    }
    if (resultStr == "0.0") {
      result = 0.0F;
      return true;
    }

    // PGN result format
    if (resultStr == "1-0") {
      result = 1.0F;
      return true;
    }
    if (resultStr == "1/2-1/2") {
      result = 0.5F;
      return true;
    }
    if (resultStr == "0-1") {
      result = 0.0F;
      return true;
    }

    return false;
  }

} // namespace tuning
