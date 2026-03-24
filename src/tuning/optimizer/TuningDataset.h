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

#ifndef FRANKYCPP_TUNINGDATASET_H
#define FRANKYCPP_TUNINGDATASET_H

//=============================================================================
// TuningDataset.h - Labeled Position Dataset for Texel Tuning
//=============================================================================
//
// Loads and manages a collection of TuningEntry objects (FEN + game result)
// for use by the Texel tuner. Supports two input formats:
//
//   1. FrankyCPP format: <FEN> [<result>]
//      Example: rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]
//
//   2. EPD format with c9 tag: <FEN-without-counters> c9 "<PGN-result>";
//      Example: rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - c9 "1-0";
//
// Provides deterministic train/test splitting (by file order, no shuffling)
// and iteration support for the optimizer.
//
// Usage:
//   TuningDataset dataset;
//   dataset.loadFromFile("positions.txt");
//   auto [train, test] = dataset.split(0.8f);
//   for (const auto& entry : train) { ... }
//
// Thread Safety:
//   TuningDataset is NOT thread-safe for modification. Read access from
//   multiple threads is safe after loading is complete.
//
//=============================================================================

#include "tuning/optimizer/TuningEntry.h"

#include <string>
#include <utility>
#include <vector>

namespace chess { class Position; }

namespace tuning {

  /// Statistics collected during dataset loading.
  struct DatasetLoadStats {
    int totalLines       = 0; ///< Total non-empty lines read from file
    int parsedOk         = 0; ///< Lines successfully parsed into TuningEntry
    int skippedEmpty     = 0; ///< Empty or whitespace-only lines
    int skippedComment   = 0; ///< Lines starting with '#' or '//'
    int skippedMalformed = 0; ///< Lines that failed to parse (bad FEN, missing result, etc.)
  };

  /// A collection of labeled positions for Texel tuning.
  class TuningDataset {

    std::vector<TuningEntry> entries;
    DatasetLoadStats loadStats{};

  public:
    /// Loads positions from a file. Supports both FrankyCPP format and EPD c9 format.
    /// Auto-detects the format per line.
    /// @param path        Path to the dataset file
    /// @param maxEntries  Maximum entries to load (0 = unlimited, default)
    /// @throws std::runtime_error if the file cannot be opened
    void loadFromFile(const std::string& path, std::size_t maxEntries = 0);

    /// Splits the dataset into train and test sets (deterministic, by position order).
    /// The first `trainFraction` of entries go to the train set, the rest to test.
    /// @param trainFraction  Fraction of entries for the train set (0.0 to 1.0, default 0.8)
    /// @return Pair of (train, test) datasets
    [[nodiscard]] std::pair<TuningDataset, TuningDataset> split(float trainFraction = 0.8F) const;

    /// Returns the number of entries in the dataset.
    [[nodiscard]] std::size_t size() const { return entries.size(); }

    /// Returns true if the dataset is empty.
    [[nodiscard]] bool empty() const { return entries.empty(); }

    /// Access an entry by index (const).
    /// @param i  Index of the entry
    /// @return Const reference to the entry
    [[nodiscard]] const TuningEntry& operator[](const std::size_t i) const { return entries[i]; }

    /// Access an entry by index (mutable — needed for activation flag computation).
    /// @param i  Index of the entry
    /// @return Mutable reference to the entry
    [[nodiscard]] TuningEntry& operator[](const std::size_t i) { return entries[i]; }

    /// Returns the loading statistics from the last loadFromFile() call.
    [[nodiscard]] const DatasetLoadStats& getLoadStats() const { return loadStats; }

    /// Returns a const reference to the underlying entry vector (for parallel iteration).
    [[nodiscard]] const std::vector<TuningEntry>& getEntries() const { return entries; }

    /// Returns a mutable reference to the underlying entry vector.
    [[nodiscard]] std::vector<TuningEntry>& getEntries() { return entries; }

    /// Iterator support for range-based for loops.
    [[nodiscard]] auto begin() const { return entries.begin(); }
    [[nodiscard]] auto end() const { return entries.end(); }
    [[nodiscard]] auto begin() { return entries.begin(); }
    [[nodiscard]] auto end() { return entries.end(); }

    /// Reserves capacity for N entries (avoids reallocations during load).
    void reserve(const std::size_t n) { entries.reserve(n); }

  private:
    /// Parses a single line in FrankyCPP format: <FEN> [<result>]
    /// @param line      The input line
    /// @param entry     Output entry (populated on success)
    /// @param validator Reusable Position for FEN validation (avoids repeated 33KB stack alloc)
    /// @return true if parsing succeeded
    [[nodiscard]] static bool parseFrankyCppFormat(const std::string& line, TuningEntry& entry,
                                                   chess::Position& validator);

    /// Parses a single line in EPD c9 format: <FEN-fields> c9 "<PGN-result>";
    /// @param line      The input line
    /// @param entry     Output entry (populated on success)
    /// @param validator Reusable Position for FEN validation (avoids repeated 33KB stack alloc)
    /// @return true if parsing succeeded
    [[nodiscard]] static bool parseEpdFormat(const std::string& line, TuningEntry& entry,
                                             chess::Position& validator);

    /// Converts a PGN result string to a float from White's perspective.
    /// @param resultStr  "1-0", "1/2-1/2", "0-1", "1.0", "0.5", "0.0"
    /// @param result     Output float (1.0, 0.5, or 0.0)
    /// @return true if the result string was recognized
    [[nodiscard]] static bool parseResult(const std::string& resultStr, float& result);
  };

} // namespace tuning

#endif // FRANKYCPP_TUNINGDATASET_H
