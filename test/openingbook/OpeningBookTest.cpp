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

#include <filesystem>

#include "init.h"
#include "types/types.h"
#include "common/Logging.h"
#include "chesscore/Position.h"
#include "chesscore/MoveGenerator.h"
#include "openingbook/OpeningBook.h"

#include <fstream>
#include <gtest/gtest.h>
using testing::Eq;

class OpeningBookTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().BOOK_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

void printEntry(BookEntry* next, int ply);

TEST_F(OpeningBookTest, readFile) {
  std::string filePathStr = "./books/superbook.pgn";
  OpeningBook book{filePathStr, OpeningBook::BookFormat::PGN};
  book.fileSize = OpeningBook::getFileSize(filePathStr);
  fprintln("Size: {:L} Byte", book.fileSize);
  std::ifstream file(filePathStr);
  std::vector<std::string> lines = book.getLinesFromFile(file);
  file.close();
  EXPECT_EQ(2'238'553, lines.size());
}

TEST_F(OpeningBookTest, initSimpleSmall) {
  std::string filePathStr = "./books/book_smalltest.txt";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::SIMPLE);
  book.setUseCache(false);
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(11'196, book.size());
}

TEST_F(OpeningBookTest, initSimple) {
#ifndef NDEBUG
  GTEST_SKIP();
#endif
  std::string filePathStr = "./books/book.txt";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::SIMPLE);
  book.setUseCache(false);
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(273'578, book.size());
}

TEST_F(OpeningBookTest, initSAN) {
  std::string filePathStr = "./books/book_graham.txt";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::SAN);
  book.setUseCache(false);
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(1'256, book.size());
}

TEST_F(OpeningBookTest, pgnECOE) {
  std::string filePathStr = "./books/ecoe.pgn";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::PGN);
  book.setUseCache(false);
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(4'039, book.size());
}

TEST_F(OpeningBookTest, initPGNSmall) {
  std::string filePathStr = "./books/pgn_test.pgn";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::PGN);
  book.setUseCache(false);
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(1'435, book.size());
}

TEST_F(OpeningBookTest, initPGNMedium) {
#ifndef NDEBUG
  GTEST_SKIP();
#endif
  std::string filePathStr = "./books/8moves_GM_LB.pgn";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::PGN);
  book.setUseCache(false);
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(204'513, book.size());
  //  [16:24:12:523075] [t:42948     ] [Book_Logger      ] [info    ]: Found 44.624 games in 2.232 ms
  //  [16:24:12:523087] [t:42948     ] [Book_Logger      ] [debug   ]: Number of games 44.624
  //  [16:24:18:586585] [t:42948     ] [Book_Logger      ] [debug   ]: Internal book created 216.070 positions in 8.309 ms.
  //  [16:24:18:594641] [t:42948     ] [Book_Logger      ] [info    ]: Opening book initialized in (8.483 ms). 216.070 positions
  //  [16:24:18:594656] [t:42948     ] [Test_Logger      ] [debug   ]: Entries in book: 216.070

  //  [16:27:22:668079] [t:26212     ] [Book_Logger      ] [info    ]: Found 44.624 games in 7.248 ms
  //  [16:27:22:668095] [t:26212     ] [Book_Logger      ] [debug   ]: Finished finding games true
  //  [16:27:22:668175] [t:41824     ] [Book_Logger      ] [debug   ]: Found games 0
  //  [16:27:22:668185] [t:41824     ] [Book_Logger      ] [debug   ]: Finished processing games.
  //  [16:27:22:668202] [t:41824     ] [Book_Logger      ] [debug   ]: Closed down ThreadPool
  //  [16:27:22:694385] [t:41824     ] [Book_Logger      ] [debug   ]: Internal book created 216.070 positions in 7.293 ms.
  //  [16:27:22:703623] [t:41824     ] [Book_Logger      ] [info    ]: Opening book initialized in (7.409 ms). 216.070 positions
  //  [16:27:22:703637] [t:41824     ] [Test_Logger      ] [debug   ]: Entries in book: 216.070
}

TEST_F(OpeningBookTest, initPGNLarge) {
//  GTEST_SKIP();
#ifndef NDEBUG
  GTEST_SKIP();
#endif
  std::string filePathStr = "./books/superbook.pgn";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::PGN);
  book.setUseCache(false);
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(4'821'485, book.size());
  // NON FIFO
  //[16:42:28:673870] [t:2688      ] [Book_Logger      ] [info    ]: Found 190.783 games in 19.856 ms
  //[16:42:28:673892] [t:2688      ] [Book_Logger      ] [debug   ]: Processing 190.783 games
  //[16:43:19:058945] [t:2688      ] [Book_Logger      ] [info    ]: Processed 190.783 games in 50.385 ms
  //[16:43:19:149450] [t:2688      ] [Book_Logger      ] [debug   ]: Internal book created 4.896.703 positions in 70.440 ms.
  //[16:43:19:225767] [t:2688      ] [Book_Logger      ] [info    ]: Opening book initialized in (71.525 ms). 4.896.703 positions
  //[16:43:19:225780] [t:2688      ] [Test_Logger      ] [debug   ]: Entries in book: 4.896.703
  //FIFO
  //[17:39:35:811577] [t:40784     ] [Book_Logger      ] [info    ]: Found 190.783 games in 53.892 ms
  //[17:39:35:811593] [t:40784     ] [Book_Logger      ] [debug   ]: Finished finding games true
  //[17:39:42:165441] [t:5988      ] [Book_Logger      ] [debug   ]: Internal book created 4.896.703 positions in 60.373 ms.
  //[17:39:42:245032] [t:5988      ] [Book_Logger      ] [info    ]: Opening book initialized in (61.519 ms). 4.896.703 positions
  //[17:39:42:245043] [t:5988      ] [Test_Logger      ] [debug   ]: Entries in book: 4.896.703
}

TEST_F(OpeningBookTest, getMove) {
  std::string filePathStr = "./books/book_smalltest.txt";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::SIMPLE);
  book.setUseCache(false);
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(11'196, book.size());

  Position position;
  MoveGenerator mg;
  Move bookMove = book.getRandomMove(position.getZobristKey());
  LOG__DEBUG(Logger::get().TEST_LOG, "Book returned move: {}", strVerbose(bookMove));
  EXPECT_TRUE(validMove(bookMove));
  EXPECT_TRUE(mg.validateMove(position, bookMove));

  position = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/6R1/pbp2PPP/1R4K1 b kq e3");
  bookMove = book.getRandomMove(position.getZobristKey());
  LOG__DEBUG(Logger::get().TEST_LOG, "Book returned move: {}", strVerbose(bookMove));
  EXPECT_FALSE(validMove(bookMove));
}

void testCache(OpeningBook &book) {
  LOG__DEBUG(Logger::get().TEST_LOG, "Load book w/o cache...");
  book.setRecreateCache(true);
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());

  NEWLINE;
  LOG__DEBUG(Logger::get().TEST_LOG, "Reset book ...");
  book.reset();

  NEWLINE;
  LOG__DEBUG(Logger::get().TEST_LOG, "Load book with cache...");
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());

  NEWLINE;
  MoveGenerator mg;
  Position position;
  Move bookMove = book.getRandomMove(position.getZobristKey());
  LOG__DEBUG(Logger::get().TEST_LOG, "Book returned move: {}", strVerbose(bookMove));
  EXPECT_TRUE(validMove(bookMove));
  EXPECT_TRUE(mg.validateMove(position, bookMove));

  position.doMove(mg.getMoveFromUci(position, "e2e4"));
  bookMove = book.getRandomMove(position.getZobristKey());
  LOG__DEBUG(Logger::get().TEST_LOG, "Book returned move: {}", strVerbose(bookMove));
  EXPECT_TRUE(validMove(bookMove));
  EXPECT_TRUE(mg.validateMove(position, bookMove));
}

TEST_F(OpeningBookTest, serializationSmall) {
  std::string filePathStr = "./books/book_smalltest.txt";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::SIMPLE);
  testCache(book);
}

TEST_F(OpeningBookTest, serializationMedium) {
#ifndef NDEBUG
  GTEST_SKIP();
#endif
  std::string filePathStr = "./books/8moves_GM_LB.pgn";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::PGN);
  testCache(book);
}

TEST_F(OpeningBookTest, serializationLarge) {
  GTEST_SKIP();
#ifndef NDEBUG
  GTEST_SKIP();
#endif
  std::string filePathStr = "./books/superbook.pgn";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::PGN);
  testCache(book);
}




