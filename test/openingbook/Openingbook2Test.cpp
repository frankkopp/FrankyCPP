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


#include "openingbook2/OpeningBook2.h"
#include "common/Logging.h"
#include "init.h"
#include "types/types.h"

#include <gtest/gtest.h>
using testing::Eq;

class OpeningBook2Test : public ::testing::Test {
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

TEST_F(OpeningBook2Test, readFile) {

  // set up Opening Book
  OpeningBook2 book{"./books/superbook.pgn", OpeningBook2::BookFormat::PGN};
  EXPECT_EQ(1, book.bookMap.size());
  EXPECT_TRUE(OpeningBook2::fileExists(book.bookFilePath));
  fprintln("File {} Size {:L} Byte", book.bookFilePath, OpeningBook2::getFileSize(book.bookFilePath));

  // non existing file
  EXPECT_THROW(book.readFile("Invalid"), std::runtime_error);

  // read file lines
  auto lines = book.readFile(book.bookFilePath);
  EXPECT_EQ(2'238'560, lines.size());
  fprintln("Lines: {:L}", lines.size());
}

TEST_F(OpeningBook2Test, readGamesSimple) {
  // set up Opening Book
  OpeningBook2 book{"./books/book.txt", OpeningBook2::BookFormat::SIMPLE};
  auto lines = book.readFile(book.bookFilePath);
  auto games = book.readGames(lines);
  fprintln("Games: {:L}", games.size());
  EXPECT_EQ(61'217, games.size());
}
