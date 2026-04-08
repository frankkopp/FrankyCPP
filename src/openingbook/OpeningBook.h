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

#ifndef FRANKYCPP_OPENINGBOOK_H
#define FRANKYCPP_OPENINGBOOK_H

#include "bookentry.h"
#include "chesscore/Position.h"
#include "common/pgn/PgnParser.h"
#include "types/types.h"
#include "version.h"

#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>

#include <filesystem>
#include <format>
#include <mutex>
#include <thread>

#include "common/gtest_friends.h"

// Forward-declare test classes at global scope so FRIEND_TEST_NS inside namespace book works
FRIEND_TEST_FWD_DECL(OpeningBookTest, readFile);
FRIEND_TEST_FWD_DECL(OpeningBookTest, readGamesSimple);
FRIEND_TEST_FWD_DECL(OpeningBookTest, readGamesSan);
FRIEND_TEST_FWD_DECL(OpeningBookTest, readGamesPgn);
FRIEND_TEST_FWD_DECL(OpeningBookTest, readGamesPgnLarge);
FRIEND_TEST_FWD_DECL(OpeningBookTest, readGamesPgnXLLarge);
FRIEND_TEST_FWD_DECL(OpeningBookTest, pgnCleanUpTest);
FRIEND_TEST_FWD_DECL(OpeningBookTest, getBookMoveVarietyZero);

namespace book {
  using namespace chess;

  typedef std::vector<std::string> Moves;

  /**
   * The OpeningBook reads game databases of different formats into an internal
   * data structure. It can then be queried for a book move on a certain position.
   * <p/>
   * Supported formats are currently:<br/>
   * BookFormat::SIMPLE for files storing a game per line with from-square and
   * to-square notation<br/>
   * BookFormat::SAN for files with lines of moves in SAN notation<br/>
   * BookFormat::PGN for PGN formatted games<br/>
   * <p/>
   * As reading these formats can be slow the OpeningBook keeps a cache file where
   * it stores the serialized data of the internal book.
   */
  class OpeningBook {
  public:
    /**
     * Supported formats are currently:<br/>
     * BookFormat::SIMPLE for files storing a game per line with from-square and
     * to-square notation<br/>
     * BookFormat::SAN for files with lines of moves in SAN notation<br/>
     * BookFormat::PGN for PGN formatted games<br/>
     * TODO: ABK format
     */
    enum class BookFormat {
      SIMPLE,
      SAN,
      PGN
    };

  private:
    // the book data structure
    std::unordered_map<ZobristKey, BookEntry> bookMap{};

    // book information
    BookFormat bookFormat{};
    std::string bookFilePath{};

    // data read from file
    std::unique_ptr<char[]> data;

    // avoid multiple initializations
    bool isInitialized = false;

    // multi threading handling
    unsigned int numberOfThreads = 1;
    std::mutex bookMutex;

    // cache control
    bool _useCache      = true;
    bool _recreateCache = false;
    // the extension cache files use after the given opening book filename
    // includes platform tag to avoid cross-platform serialization issues
#if defined(_WIN32) || defined(_WIN64)
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string platformTag = "win";
#elif defined(__linux__)
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string platformTag = "linux";
#elif defined(__APPLE__)
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string platformTag = "macos";
#else
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string platformTag = "unknown";
#endif
    const std::string cacheExt = std::format(".cache.v{}.{}.{}.bin", FrankyCPP_VERSION_MAJOR, FrankyCPP_VERSION_MINOR, platformTag);

    // the root position's zobrist key is required often - so we cache it here
    const ZobristKey rootZobristKey = Position{}.getZobristKey();

  public:
    /**
     * Creates an instance of an OpeningBook. Will not initialize (read book data).
     * Call initialize() to read book data from file or cache.<br/>
     * Supported formats are currently:<br/>
     * BookFormat::SIMPLE for files storing a game per line with from-square and
     * to-square notation<br/>
     * BookFormat::SAN for files with lines of moves in SAN notation<br/>
     * BookFormat::PGN for PGN formatted games<br/>
     */
    OpeningBook(std::string bookPath, BookFormat bFormat);

    /**
     * Initializes this OpeningBook instance by reading moves data from the file
     * given to the constructor or from cache. Also creates a cache file after
     * building the internal data structure when no cache was available.
     */
    void initialize();

    /**
     * Resets the OpeningBook to an un-initialized state. All data will be removed.
     */
    void reset();

    /**
     * Returns the number of positions in the book
     */
    uint64_t size() const { return bookMap.size(); }

    /**
     * returns a hierarchical string of the book entries with given depth
     */
    std::string str(int level);
    std::string getLevelStr(int level, int maxLevel, const BookEntry* node);

    /**
     * Returns a random move for the given position.
     * @param zobrist key of the position
     */
    Move getRandomMove(ZobristKey zobrist) const;

    /**
     * Returns a book move for the given position using frequency-weighted selection.
     * Moves leading to positions seen more often in the book source are preferred.
     * The variety parameter controls randomness: 0 = always pick highest frequency,
     * 100 = pure uniform random (like getRandomMove).
     * @param zobrist  Zobrist key of the position
     * @param variety  Randomness level (0–100, default 30)
     * @return selected book move, or MOVE_NONE if position not in book
     */
    Move getBookMove(ZobristKey zobrist, int variety = 30) const;

    // Converts a string to a BookFormat enum value. Defaults to SIMPLE
    static BookFormat fromString(const std::string& str) {
      if (str == "SAN") {
        return BookFormat::SAN;
      }
      if (str == "PGN") {
        return BookFormat::PGN;
      }
      // default
      return BookFormat::SIMPLE;
    }

  private:
    // reads all lines from a file into a vector of string_views
    std::vector<std::string_view> readFile(const std::string& filePath);

    // decides which process to use to read games based on book format and calls this process
    void readGames(const std::vector<std::string_view>& lines);

    // processes lines with one line per game and 4 chars per move without any separator characters
    // Example:
    //  g1f3c7c5e2e4d7d6d2d4c5d4f3d4g8f6b1c3b8c6f1c4d8b6d4b5a7a6c1e3b6a5b5d4e7e6e1g1f8e7
    //  e2e4g7g6d2d4f8g7b1c3d7d6f2f4c7c6g1f3d8b6f1c4g8h6c4b3c8g4d4d5a7a5a2a4b8a6h2h3g4f3
    void readGamesSimple(const std::vector<std::string_view>& lines);
    void readOneGameSimple(const std::string_view& lineView);

    // processes lines with one line per game with SAN notation
    // Example:
    //  1. f4 d5 2. Nf3 Nf6 3. e3 g6 4. b3 Bg7 5. Bb2 O-O 6. Be2 c5 7. O-O Nc6 8. Ne5 Qc7 1/2-1/2
    //  1. f4 d5 2. Nf3 Nf6 3. e3 Bg4 4. Be2 e6 5. O-O Bd6 6. b3 O-O 7. Bb2 c5 1/2-1/2
    void readGamesSan(const std::vector<std::string_view>& lines);
    void readOneGameSan(const std::string_view& lineView);

    // processes lines with one or more games in PGN format using the shared PgnParser.
    // Parses games from lines, then adds each game's moves to the book map.
    void readGamesPgn(const std::vector<std::string_view>& lines);

    // adding moves from one game to book map
    void addGameToBook(const Moves& game);

    // writing to the book map with synchronization
    void writeToBook(Move move, ZobristKey currentKey, ZobristKey lastKey);

    // Delegates to PgnParser::cleanUpMoveSection — kept for backward compatibility
  public:
    static void cleanUpPgnMoveSection(std::string& str) {
      common::pgn::PgnParser::cleanUpMoveSection(str);
    }

    // std::thread::hardware_concurrency() is not reliable - on some platforms
    // it returns 0 - in this case we chose a default of 4
    // -2 to avoid overloading the system with too many threads and to leave some resources for other processes
    static unsigned int getNoOfThreads() {
      return std::max(std::thread::hardware_concurrency(), 6U) - 2;
    }

    // checks if a cache file exists
    bool hasCache() const;

    // saves the book to a cache file
    void saveToCache();

    // loads the book from a cache file
    bool loadFromCache();

    FRIEND_TEST_NS(OpeningBookTest, readFile);
    FRIEND_TEST_NS(OpeningBookTest, readGamesSimple);
    FRIEND_TEST_NS(OpeningBookTest, readGamesSan);
    FRIEND_TEST_NS(OpeningBookTest, readGamesPgn);
    FRIEND_TEST_NS(OpeningBookTest, readGamesPgnLarge);
    FRIEND_TEST_NS(OpeningBookTest, readGamesPgnXLLarge);
    FRIEND_TEST_NS(OpeningBookTest, pgnCleanUpTest);
    FRIEND_TEST_NS(OpeningBookTest, getBookMoveVarietyZero);

    // returns if a cache is used during initialization
    constexpr bool useCache() const { return _useCache; }

    // sets if a cache is used during initialization
    void constexpr setUseCache(const bool aBool) { _useCache = aBool; }

    // returns true if the cache file will be regenerated during
    // initialization even if it already exists
    constexpr bool recreateCache() const { return _recreateCache; }

    // sets if the cache file will be regenerated during initialization
    void constexpr setRecreateCache(const bool recreateCache) { _recreateCache = recreateCache; }
  };

} // namespace book

#endif // FRANKYCPP_OPENINGBOOK_H
