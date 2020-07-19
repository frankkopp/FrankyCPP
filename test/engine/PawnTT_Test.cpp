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

#include <random>

#include "common/Logging.h"
#include "engine/PawnTT.h"
#include "init.h"
#include "chesscore/Position.h"

#include <gtest/gtest.h>
using testing::Eq;

class PawnTT_Test : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().EVAL_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(PawnTT_Test, entrySize) {
  struct EntryTest {
    Key key          = 0;
    int16_t midvalue = 0;
    int16_t endvalue = 0;
  };
  LOG__INFO(Logger::get().TEST_LOG, "Entry size = {} Byte", sizeof(EntryTest));
}

TEST_F(PawnTT_Test, basic) {
 PawnTT pawnTt{};
  LOG__INFO(Logger::get().TEST_LOG, "Trying to create a PawnTT with {:n} MB in size (default)", PawnTT::DEFAULT_TT_SIZE);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", pawnTt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:n}", pawnTt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", pawnTt.getNumberOfEntries());
  ASSERT_EQ(131072, pawnTt.getMaxNumberOfEntries());
  ASSERT_EQ(0, pawnTt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, zero) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to create a PawnTT with {:n} MB in size", 0);
  PawnTT tt;
  tt.resize(0);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries:         {:n}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:n}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of max entries:     {:n}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries:         {:n}", tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, basic10) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the PawnTT with {:n} MB in size", 10);
  PawnTT tt(10);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:n}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getNumberOfEntries());
  ASSERT_EQ(524288, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, basic64) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the PawnTT with {:n} MB in size", 64);
  PawnTT tt(64);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:n}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getNumberOfEntries());
  ASSERT_EQ(4194304, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, basic100) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the PawnTT with {:n} MB in size", 100);
  PawnTT tt(100);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:n}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getNumberOfEntries());
  ASSERT_EQ(4194304, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, basic1000) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the PawnTT with {:n} MB in size", 1'000);
  PawnTT tt(1'000);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:n}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getNumberOfEntries());
  ASSERT_EQ(33554432, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, basic10000) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the PawnTT with {:n} MB in size", 10'000);
  PawnTT tt(10'000);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:n}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getNumberOfEntries());
  ASSERT_EQ(268435456, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, parallelClear) {
  const int sizeInMB = 4'096;
  LOG__INFO(Logger::get().TEST_LOG, "Trying to create a PawnTT with {:n} MB in size", sizeInMB);
  PawnTT tt = PawnTT(sizeInMB);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:n}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:n}", tt.getNumberOfEntries());
  ASSERT_EQ(268'435'456, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
  tt.clear();
}

TEST_F(PawnTT_Test, put) {

  PawnTT tt(10);
  ASSERT_EQ(0, tt.getNumberOfPuts());
  ASSERT_EQ(0, tt.getNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfUpdates());
  ASSERT_EQ(0, tt.getNumberOfCollisions());

  Position p{};
  Score score = {1, 11};

  tt.put(p.getPawnZobristKey(), score);

  // new entry in empty bucket at pos 0
  ASSERT_EQ(1, tt.getNumberOfPuts());
  ASSERT_EQ(1, tt.getNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfCollisions());
  ASSERT_EQ(0, tt.getNumberOfUpdates());
  ASSERT_EQ(0, tt.getNumberOfHits());
  ASSERT_EQ(0, tt.getNumberOfMisses());
  ASSERT_EQ(tt.getEntryPtr(p.getPawnZobristKey())->key, p.getPawnZobristKey());
  ASSERT_EQ(tt.getEntryPtr(p.getPawnZobristKey())->midvalue, 1);
  ASSERT_EQ(tt.getEntryPtr(p.getPawnZobristKey())->endvalue, 11);

}