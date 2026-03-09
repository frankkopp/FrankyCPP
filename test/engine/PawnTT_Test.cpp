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

#include <random>
#include <thread>
#include <vector>

#include "chesscore/Position.h"
#include "common/Logging.h"
#include "engine/PawnTT.h"
#include "init.h"

#include <gtest/gtest.h>
using testing::Eq;

using namespace engine;
using namespace chess;
using namespace common;

class PawnTT_Test : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().EVAL_LOG->set_level(spdlog::level::debug);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(PawnTT_Test, entrySize) {
  struct EntryTest {
    ZobristKey key   = 0;
    int16_t midvalue = 0;
    int16_t endvalue = 0;
  };
  LOG__INFO(Logger::get().TEST_LOG, "Entry size = {} Byte", sizeof(EntryTest));
}

TEST_F(PawnTT_Test, basic) {
  const PawnTT pawnTt{};
  LOG__INFO(Logger::get().TEST_LOG, "Trying to create a PawnTT with {:L} MB in size (default)", PawnTT::DEFAULT_TT_SIZE);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", pawnTt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", pawnTt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", pawnTt.getNumberOfEntries());
  ASSERT_EQ(131072, pawnTt.getMaxNumberOfEntries());
  ASSERT_EQ(0, pawnTt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, zero) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to create a PawnTT with {:L} MB in size", 0);
  const PawnTT tt(0);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries:         {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of max entries:     {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries:         {:L}", tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, basic10) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the PawnTT with {:L} MB in size", 10);
  const PawnTT tt(10);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  ASSERT_EQ(524288, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, basic64) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the PawnTT with {:L} MB in size", 64);
  const PawnTT tt(64);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  ASSERT_EQ(4194304, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, basic100) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the PawnTT with {:L} MB in size", 100);
  const PawnTT tt(100);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  ASSERT_EQ(4194304, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, basic1000) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the PawnTT with {:L} MB in size", 1'000);
  const PawnTT tt(1'000);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  ASSERT_EQ(33554432, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, basic10000) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the PawnTT with {:L} MB in size", 10'000);
  const PawnTT tt(10'000);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  ASSERT_EQ(268435456, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, resize) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to create a PawnTT with {:L} MB in size", 0);
  PawnTT tt(0);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries:         {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of max entries:     {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries:         {:L}", tt.getNumberOfEntries());
  ASSERT_EQ(0, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
  tt.resize(64);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  ASSERT_EQ(4194304, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
  tt.resize(1000);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  ASSERT_EQ(33554432, tt.getMaxNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(PawnTT_Test, parallelClear) {
  constexpr int sizeInMB = 4'096;
  LOG__INFO(Logger::get().TEST_LOG, "Trying to create a PawnTT with {:L} MB in size", sizeInMB);
  auto tt = PawnTT(sizeInMB);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
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
  Score score = {Value{1}, Value{11}};

  tt.put(tt.getEntryPtr(p.getPawnZobristKey()), p.getPawnZobristKey(), score);

  // new entry in empty bucket at pos 0
  ASSERT_EQ(1, tt.getNumberOfPuts());
  ASSERT_EQ(1, tt.getNumberOfEntries());
  ASSERT_EQ(0, tt.getNumberOfCollisions());
  ASSERT_EQ(0, tt.getNumberOfUpdates());
  ASSERT_EQ(0, tt.getNumberOfHits()); // probe() counts hits
  ASSERT_EQ(0, tt.getNumberOfMisses());

  // Use probe() for thread-safe copy-on-read pattern
  const auto entry = tt.probe(p.getPawnZobristKey());
  ASSERT_TRUE(entry.has_value());
  ASSERT_EQ(entry->midvalue, 1);
  ASSERT_EQ(entry->endvalue, 11);
}

TEST_F(PawnTT_Test, getEntryPtr_valid_even_when_zero_then_resize) {
  PawnTT tt(0);
  const Position p{};
  const auto key = p.getPawnZobristKey();
  // Even when size is zero (logically disabled), we still have a dummy slot
  ASSERT_EQ(0, tt.getMaxNumberOfEntries());
  EXPECT_NE(nullptr, tt.getEntryPtr(key));

  tt.resize(64);
  ASSERT_GT(tt.getMaxNumberOfEntries(), 0u);
  EXPECT_NE(nullptr, tt.getEntryPtr(key));
}

// =============================================================================
// Concurrent Put+Probe Stress Test for Lazy SMP Thread Safety
// =============================================================================
// This test verifies that PawnTT is safe for concurrent access under Lazy SMP.
// Multiple threads simultaneously put and probe entries with overlapping keys.
//
// What this test validates:
//   - No crash, deadlock, or ASAN errors under concurrent access
//   - Data coherence: when a probe hits (key matches), the value fields
//     were written by a valid put() — not torn/garbage data
//   - The atomic key with acquire/release semantics works correctly
//
// Run with -fsanitize=thread (TSAN) on WSL/Linux for a definitive race check.
// =============================================================================
TEST_F(PawnTT_Test, ConcurrentPutProbeNoUB) {
  constexpr int NUM_THREADS = 4;
  constexpr int ITERATIONS  = 500'000;
  constexpr int TT_SIZE_MB  = 4; // small = high collision rate = more contention

  PawnTT tt(TT_SIZE_MB);
  tt.setSmpThreads(NUM_THREADS); // signals SMP mode to suppress update warnings

  // Value ranges written by all threads — any probe hit must fall within these.
  constexpr int VALUE_LO = -1000;
  constexpr int VALUE_HI = 1000;

  const auto threadWork = [&](const int threadId) {
    std::mt19937_64 rng(static_cast<uint64_t>(threadId) * 0xDEADBEEF12345678ULL);
    std::uniform_int_distribution<ZobristKey> keyDist(1, 1'000'000);
    std::uniform_int_distribution<int> valueDist(VALUE_LO, VALUE_HI);

    for (int i = 0; i < ITERATIONS; ++i) {
      const ZobristKey key = keyDist(rng);
      const auto midvalue  = static_cast<Value>(valueDist(rng));
      const auto endvalue  = static_cast<Value>(valueDist(rng));
      const Score score    = {midvalue, endvalue};

      // Use getEntryPtr() only for put() - this is the correct pattern
      tt.put(tt.getEntryPtr(key), key, score);

      // Use probe() for thread-safe copy-on-read pattern.
      // This returns a COPY of the entry, eliminating races where another
      // thread overwrites the entry between key check and value reads.
      if (const auto entry = tt.probe(key)) {
        // Hit: probe() returned a consistent copy.
        // The release-store in put() that published this key guaranteed
        // all value fields were visible before the key was written.
        // So values must be within the range that a legitimate put() wrote.
        //
        // Note: another thread may have written to this slot between our
        // put and probe — that is valid SMP behavior. We just check that
        // whatever we read is coherent.
        EXPECT_GE(entry->midvalue, VALUE_LO);
        EXPECT_LE(entry->midvalue, VALUE_HI);
        EXPECT_GE(entry->endvalue, VALUE_LO);
        EXPECT_LE(entry->endvalue, VALUE_HI);
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(NUM_THREADS);
  for (int t = 0; t < NUM_THREADS; ++t) {
    threads.emplace_back(threadWork, t);
  }
  for (auto& t : threads) {
    t.join();
  }

  fprintln("PawnTT ConcurrentPutProbeNoUB: {} threads x {} iterations completed cleanly",
           NUM_THREADS, ITERATIONS);
  fprintln("PawnTT Stats after stress: {}", tt.str());
}
