/*
 * MIT License
 *
 * Copyright (c) 2018-2020 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
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

#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/shared_ptr.hpp>

#include "chesscore/Position.h"
#include "chesscore/MoveGenerator.h"
#include "PGN_Reader.h"

#include "gtest/gtest_prod.h"

/**
 * An entry in the opening book data structure. Stores a key (e.g. zobrist key)
 * for the position, the current fen, a count of how often a position is in the
 * book and two vectors storing the moves from the position and pointers to the
 * book entry for the corresponding move.
 */
struct BookEntry {
  Key key{};
  std::string fen{};
  int counter{0};
  std::vector<Move> moves{};
  std::vector<std::shared_ptr<BookEntry>> ptrNextPosition{};

  BookEntry() = default;
  BookEntry(const Key &zobrist, const std::string &fenString) : key(zobrist), fen(fenString), counter{1} {}
  std::string str();

  // BOOST Serialization
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, [[maybe_unused]] const unsigned int version) {
    ar & BOOST_SERIALIZATION_NVP (key);
    ar & BOOST_SERIALIZATION_NVP (fen);
    ar & BOOST_SERIALIZATION_NVP (counter);
    ar & BOOST_SERIALIZATION_NVP (moves);
    ar & BOOST_SERIALIZATION_NVP (ptrNextPosition);
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

  // multi threading handling
  std::mutex bookMutex;
  unsigned int numberOfThreads = 1;

  // book information
  BookFormat bookFormat;
  std::string bookFilePath{};
  uint64_t fileSize{};

  // progress information
  uint64_t gamesTotal = 0;
  uint64_t gamesProcessed = 0;

  // cache control
  bool _useCache = true;
  bool _recreateCache = false;

  // avoid multiple initializations
  bool isInitialized = false;

  // move generator instance to check moves
  MoveGenerator mg{};

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
  explicit OpeningBook(std::string bookPath, const BookFormat &bFormat);

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
  uint64_t size() { return bookMap.size(); }

  /**
   * Returns a random move for the given position.
   * @param zobrist key of the position
   */
  Move getRandomMove(Key zobrist) const;

private:
  void readBookFromFile(const std::string &filePath);
  std::vector<std::string> getLinesFromFile(std::ifstream &ifstream) const;
  void processAllLines(std::vector<std::string> &lines);
  void processLine(std::string &line);
  void processSimpleLine(std::string &line);
  void processSANLine(std::string &line);
  void processPGNFileFifo(std::vector<std::string> &lines);
  void processPGNFile(std::vector<std::string> &lines);
  void processGames(std::vector<PGN_Game>* ptrGames);
  void processGame(PGN_Game &game);
  void addToBook(Position &currentPosition, const Move &move);

  bool hasCache() const;
  void saveToCache();
  bool loadFromCache();

public:
  bool useCache() const { return _useCache; }
  void setUseCache(bool aBool) { _useCache = aBool; }
  bool recreateCache() const { return _recreateCache; }
  void setRecreateCache(bool recreateCache) { _recreateCache = recreateCache; }

  static uint64_t getFileSize(const std::string &filePath);
  static bool fileExists(const std::string& filePath);
};

#endif //FRANKYCPP_OPENINGBOOK_H
