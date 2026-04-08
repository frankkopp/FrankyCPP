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


#include "openingbook/OpeningBook.h"
#include "Test_Utils.h"
#include "chesscore/MoveGenerator.h"
#include "common/Logging.h"
#include "common/stringutil.h"
#include "init.h"
#include "types/types.h"

#include <gtest/gtest.h>
#include <set>
using testing::Eq;

using namespace common;
using namespace chess;
using namespace book;

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
  EXPECT_EQ(0, book.bookMap.size());
  EXPECT_TRUE(std::filesystem::exists(book.bookFilePath));
  fprintln("File {} Size {:L} Byte", book.bookFilePath, std::filesystem::file_size(book.bookFilePath));

  // read file lines
  const auto lines = book.readFile(book.bookFilePath);
  EXPECT_EQ(2'620'079, lines.size());
  fprintln("Lines: {:L}", lines.size());
}

TEST_F(OpeningBookTest, pgnCleanUpTest) {
  std::string testString{"e4(d4) d5!!2.c4$50(Nf3?)e5 Nf3{Comment !}Nc6 Nc3 Nf6 Bc4 {another comment} Bc5 O-O O-O a1=Q  @@@æææ {unexpected characters are skipped}  <> {These symbols are reserved}  1/2-1/2  ; comment     "};
  fprintln("Before: '{}'", testString);
  std::string test = removeTrailingComments(testString, ";");
  OpeningBook::cleanUpPgnMoveSection(test);
  fprintln("After : '{}'", test);
  EXPECT_EQ("e4 d5 c4 e5 Nf3 Nc6 Nc3 Nf6 Bc4 Bc5 O-O O-O a1=Q", test);
}

TEST_F(OpeningBookTest, initSimple) {
#ifndef NDEBUG
  GTEST_SKIP() << "Skipping in debug build due to duration";
#endif
  OpeningBook book("./books/book.txt", OpeningBook::BookFormat::SIMPLE);
  //  book.setRecreateCache(true);
  book.setUseCache(false);
  book.initialize();
  fprintln("Book:  {:L} entries", book.size());
  EXPECT_EQ(273'578, book.size());
}

TEST_F(OpeningBookTest, initSan) {
#ifndef NDEBUG
  GTEST_SKIP() << "Skipping in debug build due to duration";
#endif
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

TEST_F(OpeningBookTest, 8movesPgn) {
  if (isBulkRun()) GTEST_SKIP();
  OpeningBook book("./books/8moves_v3.pgn", OpeningBook::BookFormat::PGN);
  book.setUseCache(false);
  book.initialize();
  fprintln("Book:  {:L} entries", book.size());
  EXPECT_EQ(166'178, book.size());
}

TEST_F(OpeningBookTest, initPgnLarge) {
  if (isBulkRun()) GTEST_SKIP();
  OpeningBook book("./books/superbook.pgn", OpeningBook::BookFormat::PGN);
  book.setUseCache(false);
  book.initialize();
  fprintln("Book:  {:L} entries", book.size());
  EXPECT_EQ(4'821'615, book.size());
  fprintln("{}", book.str(1));
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string expected = "Root (190.780)";
  EXPECT_TRUE(book.str(1).find_first_of(expected) != std::string::npos);
}

TEST_F(OpeningBookTest, initPgnXLLarge) {
  if (isBulkRun()) GTEST_SKIP();
  // superbook_xl is a multiple self copy of the normal non xl version
  OpeningBook book("./books/superbook_xl.pgn", OpeningBook::BookFormat::PGN);
  book.setUseCache(false);
  book.initialize();
  fprintln("Book:  {:L} entries", book.size());
  EXPECT_EQ(4'821'615, book.size());
  fprintln("{}", book.str(1));
  std::string expected = "Root (3.815.600)";
  //  EXPECT_TRUE(book.str(1).starts_with(expected));
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
  LOG__DEBUG(Logger::get().TEST_LOG, "Book returned move: {}", bookMove.strVerbose());
  EXPECT_TRUE(bookMove.isValid());
  EXPECT_TRUE(mg.validateMove(position, bookMove));

  position = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/6R1/pbp2PPP/1R4K1 b kq e3");
  bookMove = book.getRandomMove(position.getZobristKey());
  LOG__DEBUG(Logger::get().TEST_LOG, "Book returned move: {}", bookMove.strVerbose());
  EXPECT_FALSE(bookMove.isValid());
}


TEST_F(OpeningBookTest, serializationSimple) {
#ifndef NDEBUG
  GTEST_SKIP() << "Skipping in debug build due to duration";
#endif
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string filePathStr = "./books/book.txt";
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
  if (isBulkRun()) GTEST_SKIP();

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
  LOG__DEBUG(Logger::get().TEST_LOG, "Book returned move: {}", bookMove.strVerbose());
  EXPECT_TRUE(bookMove.isValid());
  EXPECT_TRUE(mg.validateMove(position, bookMove));

  position.doMove(mg.getMoveFromUci(position, "e2e4"));
  bookMove = book.getRandomMove(position.getZobristKey());
  LOG__DEBUG(Logger::get().TEST_LOG, "Book returned move: {}", bookMove.strVerbose());
  EXPECT_TRUE(bookMove.isValid());
  EXPECT_TRUE(mg.validateMove(position, bookMove));
}

TEST_F(OpeningBookTest, str) {
  OpeningBook book("./books/book_smalltest.txt", OpeningBook::BookFormat::SIMPLE);
  book.setUseCache(false);
  book.initialize();
  const std::string output = book.str(1);
  fprintln("{}", output);
  std::string expected = "Root (1.000)";
  //  EXPECT_TRUE(book.str(1).starts_with(expected));
}

TEST_F(OpeningBookTest, getBookMoveValid) {
  OpeningBook book("./books/book_smalltest.txt", OpeningBook::BookFormat::SIMPLE);
  book.setUseCache(false);
  book.initialize();

  const Position position;
  MoveGenerator mg;

  // getBookMove with default variety should return a valid move from the start position
  const Move bookMove = book.getBookMove(position.getZobristKey());
  LOG__DEBUG(Logger::get().TEST_LOG, "getBookMove returned: {}", bookMove.strVerbose());
  EXPECT_TRUE(bookMove.isValid());
  EXPECT_TRUE(mg.validateMove(position, bookMove));
}

TEST_F(OpeningBookTest, getBookMoveVarietyZero) {
  OpeningBook book("./books/book_smalltest.txt", OpeningBook::BookFormat::SIMPLE);
  book.setUseCache(false);
  book.initialize();

  const Position position;
  const auto rootKey = position.getZobristKey();

  // Determine the expected set of highest-frequency moves from the book data.
  // Look up each move's destination counter and find the maximum.
  const auto& rootEntry = book.bookMap[rootKey];
  ASSERT_FALSE(rootEntry.moves.empty());

  int maxFreq = 0;
  for (const auto& nextKey : rootEntry.nextPosition) {
    const auto it = book.bookMap.find(nextKey);
    if (it != book.bookMap.end()) {
      maxFreq = std::max(maxFreq, it->second.counter);
    }
  }
  ASSERT_GT(maxFreq, 0);

  // Collect the move strings that are tied at max frequency
  std::set<std::string> expectedMoves;
  for (std::size_t i = 0; i < rootEntry.moves.size(); ++i) {
    const auto it = book.bookMap.find(rootEntry.nextPosition[i]);
    if (it != book.bookMap.end() && it->second.counter == maxFreq) {
      expectedMoves.insert(rootEntry.moves[i].str());
    }
  }
  ASSERT_FALSE(expectedMoves.empty());

  // Log what we expect
  std::string expectedList;
  for (const auto& m : expectedMoves) {
    expectedList += m + " ";
  }
  LOG__DEBUG(Logger::get().TEST_LOG, "Max frequency {} — expected moves: {}", maxFreq, expectedList);

  // With variety=0, every returned move must be one of the top-frequency moves
  std::set<std::string> seen;
  for (int i = 0; i < 100; ++i) {
    const Move move = book.getBookMove(rootKey, 0);
    ASSERT_TRUE(move.isValid());
    const auto moveStr = move.str();
    EXPECT_TRUE(expectedMoves.contains(moveStr))
      << "variety=0 returned '" << moveStr << "' which is not among the top-frequency moves";
    seen.insert(moveStr);
  }

  // Over 100 draws, we should see most (if not all) of the tied moves
  LOG__DEBUG(Logger::get().TEST_LOG, "variety=0 actually selected: {} of {} top moves",
             seen.size(), expectedMoves.size());
}

TEST_F(OpeningBookTest, getBookMoveUnknownPosition) {
  OpeningBook book("./books/book_smalltest.txt", OpeningBook::BookFormat::SIMPLE);
  book.setUseCache(false);
  book.initialize();

  // Position not in book should return MOVE_NONE
  const Position position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/6R1/pbp2PPP/1R4K1 b kq e3");
  const Move bookMove = book.getBookMove(position.getZobristKey());
  EXPECT_FALSE(bookMove.isValid());
}
