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

#ifndef FRANKYCPP_OPENINGBOOK2_H
#define FRANKYCPP_OPENINGBOOK2_H

#include <filesystem>
#include <unordered_map>

#include "openingbook2/BookEntry2.h"

#include "gtest/gtest_prod.h"

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
class OpeningBook2 {
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
  std::unordered_map<Key, BookEntry2> bookMap{};

  // book information
  BookFormat bookFormat{};
  std::string bookFilePath{};

  // data read from file
  std::unique_ptr<char[]> data;

  // avoid multiple initializations
  bool isInitialized = false;

  // multi threading handling
  unsigned int numberOfThreads = 1;

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
  OpeningBook2(std::string bookPath, BookFormat bFormat);

  /**
   * Initializes this OpeningBook instance by reading moves data from the file
    * given to the constructor or from cache. Also creates a cache file after
    * building the internal data structure when no cache was available.
    */
  void initialize();

  /**
   * Resets the OpeningBook to an un-initialized state. All data will be removed.
   */
  //    void reset();

  /**
   * Returns the number of positions in the book
   */
  uint64_t size() { return bookMap.size(); }

  /**
   * Returns a random move for the given position.
   * @param zobrist key of the position
   */
  //    [[nodiscard]] Move getRandomMove(Key zobrist) const;

  /** Checks of file exists and encapsulates platform differences for
   * filesystem operations */
  static inline bool fileExists(const std::string& filePath) {
    return std::filesystem::exists(filePath);
  }

  /** Returns files size in bytes and encapsulates platform differences for
   * filesystem operations */
  static inline uint64_t getFileSize(const std::string& filePath) {
    return std::filesystem::file_size(filePath);
  }

private:

  // reads all lines from a file into a vector of string_views
  std::vector<std::string_view> readFile(const std::string& filePath);

  // decides which process to use to read games based on book format
  std::vector<MoveList> readGames(std::vector<std::string_view> &lines);

  void readGamesSimple(const std::vector<std::string_view>& line, std::vector<MoveList>& games);
  void readGamesSan(const std::vector<std::string_view>& lines, std::vector<MoveList>& games);
  void readGamesPgn(const std::vector<std::string_view>& lines, std::vector<MoveList>& games);

  static MoveList readOneGameSimple(const std::string_view& lineView);

  
  FRIEND_TEST(OpeningBook2Test, readFile);
  FRIEND_TEST(OpeningBook2Test, readGamesSimple);
  FRIEND_TEST(OpeningBook2Test, readGamesSan);
  FRIEND_TEST(OpeningBook2Test, readGamesPgn);
};


#endif//FRANKYCPP_OPENINGBOOK2_H
