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

#ifndef FRANKYCPP_OPENINGBOOK_H
#define FRANKYCPP_OPENINGBOOK_H

#include "chesscore/Position.h"
#include "types/types.h"

#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>

#include <filesystem>
#include <mutex>
#include <thread>

#include "gtest/gtest_prod.h"

typedef std::vector<std::string> Moves;

/**
 * An entry in the opening book data structure. Stores a key (e.g. zobrist key)
 * for the position, the current fen, a count of how often a position is in the
 * book and two vectors storing the moves from the position and the key to the
 * book entry for the corresponding move.
 */
struct BookEntry {
  Key key{};
  int counter{1};
  std::vector<Move> moves{};
  std::vector<Key> nextPosition{};

  BookEntry() = default;// necessary for serialization
  explicit BookEntry(Key zobrist) : key(zobrist), counter{1} {}

  [[nodiscard]] std::string str() const {
    std::ostringstream os;
    os << this->key << " (" << this->counter << ")"
       << " [ ";
    for (std::size_t i = 0; i < moves.size(); i++) {
      os << ::str(this->moves[i]) << " ";
    }
    os << "] ";
    return os.str();
  }

  // BOOST Serialization
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
    ar& BOOST_SERIALIZATION_NVP(key);
    ar& BOOST_SERIALIZATION_NVP(counter);
    ar& BOOST_SERIALIZATION_NVP(moves);
    ar& BOOST_SERIALIZATION_NVP(nextPosition);
  }
};

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
  std::unordered_map<Key, BookEntry> bookMap{};

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
  std::mutex gamesMutex;

  // cache control
  bool _useCache      = true;
  bool _recreateCache = false;
  // the extension cache files use after the given opening book filename
  static constexpr const char* cacheExt = ".cache.bin";

  // the root position's zobrist key is required often - so we cache it here
  const Key rootZobristKey = Position{}.getZobristKey();

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
  [[nodiscard]] uint64_t size() const { return bookMap.size(); }

  /**
   * returns a hierarchical string of the book entries with given depth
   */
  [[nodiscard]] std::string str(int level);
  std::string getLevelStr(int level, int maxLevel, const BookEntry* node);

  /**
   * Returns a random move for the given position.
   * @param zobrist key of the position
   */
  [[nodiscard]] Move getRandomMove(Key zobrist) const;

  /** Checks of file exists and encapsulates platform differences for
  * filesystem operations (obsolete with support for std::filesystem) */
  static inline bool fileExists(const std::string& filePath) {
    return std::filesystem::exists(filePath);
  }

  /** Returns files size in bytes and encapsulates platform differences for
  * filesystem operations  (obsolete with support for std::filesystem) */
  static inline uint64_t getFileSize(const std::string& filePath) {
    return std::filesystem::file_size(filePath);
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

  // processes lines with one or more games in PGN format. PGN uses multiple line per game
  // for meta data and moves. Games will be extracted and the interpreted in parallel to retrieve
  // moves
  void readGamesPgn(const std::vector<std::string_view>* lines);
  void readOneGamePgn(const std::vector<std::string_view>* lines, size_t gameStart, size_t gameEnd);

  // adding moves from one game to book map
  void addGameToBook(const Moves& game);

  // writing to the book map with synchronization
  void writeToBook(Move move, Key currentKey, Key lastKey);

  // fast removal of unwanted parts of a PGN move section (not using slow std::regex)
  static void cleanUpPgnMoveSection(std::string& str);

  // std::thread::hardware_concurrency() is not reliable - on some platforms
  // it returns 0 - in this case we chose a default of 4
  static inline unsigned int getNoOfThreads() {
    return std::thread::hardware_concurrency() == 0 ? 4 : std::thread::hardware_concurrency();
  }

  // checks if a cache file exists
  [[nodiscard]] bool hasCache() const;

  // saves the book to a cache file
  void saveToCache();

  // loads the book from a cache file
  bool loadFromCache();

  FRIEND_TEST(OpeningBookTest, readFile);
  FRIEND_TEST(OpeningBookTest, readGamesSimple);
  FRIEND_TEST(OpeningBookTest, readGamesSan);
  FRIEND_TEST(OpeningBookTest, readGamesPgn);
  FRIEND_TEST(OpeningBookTest, readGamesPgnLarge);
  FRIEND_TEST(OpeningBookTest, readGamesPgnXLLarge);
  FRIEND_TEST(OpeningBookTest, pgnCleanUpTest);

public:
  // returns if a cache is used during initialization
  [[nodiscard]] bool useCache() const { return _useCache; }

  // sets if a cache is used during initialization
  void setUseCache(bool aBool) { _useCache = aBool; }

  // returns if the cache file will be regenerated during
  // initialization even if it already exists
  [[nodiscard]] bool recreateCache() const { return _recreateCache; }

  // sets if the cache file will be regenerated during initialization
  void setRecreateCache(bool recreateCache) { _recreateCache = recreateCache; }
};


#endif//FRANKYCPP_OPENINGBOOK_H
