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


#include "init.h"
#include "types/types.h"
#include "common/Logging.h"
#include "openingbook/OpeningBook.h"
#include <chesscore/MoveGenerator.h>

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

TEST_F(OpeningBookTest, readFile) {

  // set up Opening Book
  OpeningBook book{"./books/superbook.pgn", OpeningBook::BookFormat::PGN};
  EXPECT_EQ(1, book.bookMap.size());
  EXPECT_TRUE(OpeningBook::fileExists(book.bookFilePath));
  fprintln("File {} Size {:L} Byte", book.bookFilePath, OpeningBook::getFileSize(book.bookFilePath));

  // non existing file
  EXPECT_THROW(book.readFile("Invalid"), std::runtime_error);

  // read file lines
  auto lines = book.readFile(book.bookFilePath);
  EXPECT_EQ(2'620'079, lines.size());
  fprintln("Lines: {:L}", lines.size());
}

TEST_F(OpeningBookTest, pgnCleanUpTest) {
  std::string_view testView{"e4(d4) d5!!2.c4$50(Nf3?)e5 Nf3{Comment !}Nc6 Nc3 Nf6 Bc4 {another comment} Bc5 O-O O-O a1=Q  @@@æææ {unexpected characters are skipped}  <> {These symbols are reserved}  1/2-1/2  ; comment     "};
  fprintln("Before: '{}'", testView);
  std::string test = OpeningBook::removeTrailingComments(testView);
  OpeningBook::cleanUpPgnMoveSection(test);
  fprintln("After : '{}'", test);
  EXPECT_EQ("e4 d5 c4 e5 Nf3 Nc6 Nc3 Nf6 Bc4 Bc5 O-O O-O a1=Q", test);
}

TEST_F(OpeningBookTest, initSimple) {
  OpeningBook book("./books/book.txt", OpeningBook::BookFormat::SIMPLE);
  //  book.setRecreateCache(true);
  book.setUseCache(false);
  book.initialize();
  fprintln("Book:  {:L} entries", book.size());
  EXPECT_EQ(273'578, book.size());
}

TEST_F(OpeningBookTest, initSan) {
  OpeningBook book("./books/book_test.san", OpeningBook::BookFormat::SAN);
  book.setUseCache(false);
  book.initialize();
  fprintln("Book:  {:L} entries", book.size());
  EXPECT_EQ(1'256, book.size());
}

TEST_F(OpeningBookTest, initPgn) {
  OpeningBook book("./books/pgn_test.pgn", OpeningBook::BookFormat::PGN);
  book.setUseCache(false);
  book.initialize();
  fprintln("Book:  {:L} entries", book.size());
  EXPECT_EQ(1'495, book.size());
}

TEST_F(OpeningBookTest, initPgnLarge) {
  //  GTEST_SKIP();
#ifndef NDEBUG                 
  GTEST_SKIP();
#endif
  OpeningBook book("./books/superbook.pgn", OpeningBook::BookFormat::PGN);
  book.setUseCache(false);
  book.initialize();
  fprintln("Book:  {:L} entries", book.size()); 
  EXPECT_EQ(4'821'615, book.size());
  fprintln("{}", book.str(1));
  std::string expected = "Root (190.780)";
  EXPECT_TRUE(book.str(1).starts_with(expected));
}

TEST_F(OpeningBookTest, initPgnXLLarge) {
//  GTEST_SKIP();
#ifndef NDEBUG
  GTEST_SKIP();
#endif
  // superbook_xl is a multiple self copy of the normal non xl version
  OpeningBook book("./books/superbook_xl.pgn", OpeningBook::BookFormat::PGN);
  book.setUseCache(false);
  book.initialize();
  fprintln("Book:  {:L} entries", book.size());
  EXPECT_EQ(4'821'615, book.size());
  fprintln("{}", book.str(1));
  std::string expected = "Root (3.815.600)";
  EXPECT_TRUE(book.str(1).starts_with(expected));
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


TEST_F(OpeningBookTest, serializationSimple) {
  //  GTEST_SKIP();
#ifndef NDEBUG
  GTEST_SKIP();
#endif
  std::string filePathStr = "./books/book.txt";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::SIMPLE);

  LOG__DEBUG(Logger::get().TEST_LOG, "Load book w/o cache...");
  book.setRecreateCache(true);
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(273578, book.size());

  NEWLINE;
  LOG__DEBUG(Logger::get().TEST_LOG, "Reset book ...");
  book.reset();

  NEWLINE;
  LOG__DEBUG(Logger::get().TEST_LOG, "Load book with cache...");
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(273578, book.size());
}

TEST_F(OpeningBookTest, serializationLarge) {
  //  GTEST_SKIP();
#ifndef NDEBUG
  GTEST_SKIP();
#endif
  std::string filePathStr = "./books/superbook.pgn";
  OpeningBook book(filePathStr, OpeningBook::BookFormat::PGN);

  LOG__DEBUG(Logger::get().TEST_LOG, "Load book w/o cache...");
  book.setRecreateCache(true);
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(4'821'615, book.size());

  NEWLINE;
  LOG__DEBUG(Logger::get().TEST_LOG, "Reset book ...");
  book.reset();

  NEWLINE;
  LOG__DEBUG(Logger::get().TEST_LOG, "Load book with cache...");
  book.initialize();
  LOG__DEBUG(Logger::get().TEST_LOG, "Entries in book: {:L}", book.size());
  EXPECT_EQ(4'821'615, book.size());

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

TEST_F(OpeningBookTest, str) {
  OpeningBook book("./books/book_smalltest.txt", OpeningBook::BookFormat::SIMPLE);
  book.setUseCache(false);
  book.initialize();
  const std::string output = book.str(1);
  fprintln("{}", output);
  std::string expected = "Root (1.000)";
  EXPECT_TRUE(book.str(1).starts_with(expected));
}
