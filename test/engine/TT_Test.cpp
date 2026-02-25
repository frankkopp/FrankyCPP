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

#include "common/Logging.h"
#include "engine/TT.h"
#include "init.h"
#include "Test_Utils.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
using testing::Eq;


class TT_Test : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().TT_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TT_Test, entrySize) {
  struct EntryTest {
    // sorted by size to achieve smallest struct size
    // using bitfield for smallest size
    ZobristKey key       = 0;         // 64 bit
    uint16_t move = 0;         // MOVE_NONE as 16-bit
    Value eval    = VALUE_NONE;// 16 bit signed
    Value value   = VALUE_NONE;// 16 bit signed
    int8_t depth : 7;          // 0-127
    uint8_t age : 3;           // 0-7
    ValueType type : 2;        // 4 values
    bool mateThreat : 1;       // 1-bit bool
  };
  LOG__INFO(Logger::get().TEST_LOG, "Entry size = {} Byte", sizeof(EntryTest));
}

TEST_F(TT_Test, basic) {
  const TT tt;
  LOG__INFO(Logger::get().TEST_LOG, "Trying to create a TT with {:L} MB in size (default)", TT::DEFAULT_TT_SIZE);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  EXPECT_EQ(131072, tt.getMaxNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(TT_Test, basic10) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the TT with {:L} MB in size", 10);
  const TT tt(10);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  EXPECT_EQ(524288, tt.getMaxNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(TT_Test, basic100) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the TT with {:L} MB in size", 100);
  const TT tt(100);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  EXPECT_EQ(4194304, tt.getMaxNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(TT_Test, basic1000) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the TT with {:L} MB in size", 1'000);
  const TT tt(1'000);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  EXPECT_EQ(33554432, tt.getMaxNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(TT_Test, basic10000) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the TT with {:L} MB in size", 10'000);
  EXPECT_NO_THROW(TT tt(10'000););
  //  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  //  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  //  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  //  EXPECT_EQ(536870912, tt.getMaxNumberOfEntries());
  //  EXPECT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(TT_Test, basic32000) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the TT with {:L} MB in size", 32'000);
  const TT tt(32'000);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  EXPECT_EQ(1073741824, tt.getMaxNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(TT_Test, basic36000) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the TT with {:L} MB in size", 32'000);
  const TT tt(36'000);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  EXPECT_EQ(2147483648, tt.getMaxNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(TT_Test, basic64) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to resize the TT with {:L} MB in size", 64);
  const TT tt(64);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  EXPECT_EQ(4194304, tt.getMaxNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(TT_Test, zero) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to create a TT with {:L} MB in size", 0);
  const TT tt(0);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries:         {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of max entries:     {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries:         {:L}", tt.getNumberOfEntries());
}

TEST_F(TT_Test, resize) {
  LOG__INFO(Logger::get().TEST_LOG, "Trying to create a TT with {:L} MB in size", 0);
  TT tt(0);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries:         {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of max entries:     {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries:         {:L}", tt.getNumberOfEntries());
  EXPECT_EQ(1, tt.getMaxNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfEntries());
  tt.resize(64);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  EXPECT_EQ(4194304, tt.getMaxNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfEntries());
  tt.resize(1'000);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  EXPECT_EQ(33554432, tt.getMaxNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfEntries());
}

TEST_F(TT_Test, parallelClear) {
  constexpr int sizeInMB = 4'096;
  LOG__INFO(Logger::get().TEST_LOG, "Trying to create a TT with {:L} MB in size", sizeInMB);
  auto tt = TT(sizeInMB);
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getMaxNumberOfEntries());
  LOG__INFO(Logger::get().TEST_LOG, "Number of bytes allocated: {:L}", tt.getSizeInByte());
  LOG__INFO(Logger::get().TEST_LOG, "Number of entries: {:L}", tt.getNumberOfEntries());
  EXPECT_EQ(268'435'456, tt.getMaxNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfEntries());
  tt.setThreads(8);
  tt.clear();
}

TEST_F(TT_Test, put) {
  std::random_device rd;
  std::mt19937_64 rg(rd());
  std::uniform_int_distribution<unsigned long long> randomKey;

  TT tt(10);

  uint64_t collisionDistance = tt.maxNumberOfEntries;

  const ZobristKey key1 = randomKey(rg);
  const ZobristKey key2 = key1 + 13;               // different bucket
  const ZobristKey key3 = key1 + collisionDistance;// same bucket - collision

  // new entry in empty bucket at pos 0
  tt.put(key1, static_cast<Depth>(6), Move(SQ_E2, SQ_E4), static_cast<Value>(101), EXACT, static_cast<Value>(1001));
  EXPECT_EQ(1, tt.getNumberOfPuts());
  EXPECT_EQ(1, tt.getNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfUpdates());
  EXPECT_EQ(0, tt.getNumberOfCollisions());
  EXPECT_EQ(0, tt.getNumberOfOverwrites());
  EXPECT_EQ(tt.getMatch(key1)->key, key1);
  EXPECT_EQ(tt.getMatch(key1)->value, static_cast<Value>(101));
  EXPECT_EQ(tt.getMatch(key1)->eval, static_cast<Value>(1001));

  // new entry
  tt.put(key2, static_cast<Depth>(5), Move(SQ_E2, SQ_E4), static_cast<Value>(102), EXACT, static_cast<Value>(1002));
  EXPECT_EQ(2, tt.getNumberOfPuts());
  EXPECT_EQ(2, tt.getNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfUpdates());
  EXPECT_EQ(0, tt.getNumberOfCollisions());
  EXPECT_EQ(0, tt.getNumberOfOverwrites());
  EXPECT_EQ(tt.getMatch(key2)->key, key2);
  EXPECT_EQ(tt.getMatch(key2)->value, static_cast<Value>(102));
  EXPECT_EQ(tt.getMatch(key2)->eval, static_cast<Value>(1002));
  EXPECT_EQ(tt.getMatch(key2)->depth, static_cast<Value>(5));


  // new entry (collision)
  tt.put(key3, static_cast<Depth>(6), Move(SQ_E2, SQ_E4), static_cast<Value>(103), EXACT, static_cast<Value>(1003));
  EXPECT_EQ(3, tt.getNumberOfPuts());
  EXPECT_EQ(2, tt.getNumberOfEntries());
  EXPECT_EQ(0, tt.getNumberOfUpdates());
  EXPECT_EQ(1, tt.getNumberOfCollisions());
  EXPECT_EQ(1, tt.getNumberOfOverwrites());
  EXPECT_EQ(tt.getMatch(key3)->key, key3);
  EXPECT_EQ(tt.getMatch(key3)->value, static_cast<Value>(103));
  EXPECT_EQ(tt.getMatch(key3)->eval, static_cast<Value>(1003));
}

TEST_F(TT_Test, get) {
  std::random_device rd;
  std::mt19937_64 rg(rd());
  std::uniform_int_distribution<unsigned long long> randomKey;

  TT tt(10);

  const uint64_t collisionDistance = tt.maxNumberOfEntries;

  const ZobristKey key1 = randomKey(rg);
  const ZobristKey key2 = key1 + 13;               // different bucket
  const ZobristKey key3 = key1 + collisionDistance;// same bucket - collision
  const ZobristKey key4 = key1 + 17;

  // new entry in empty slot
  tt.put(key1, static_cast<Depth>(6), Move(SQ_E2, SQ_E4), static_cast<Value>(101), EXACT, static_cast<Value>(1001));
  const TT::Entry* e1 = tt.getMatch(key1);
  EXPECT_EQ(101, e1->value);

  // new entry in empty slot
  tt.put(key2, static_cast<Depth>(5), Move(SQ_E2, SQ_E4), static_cast<Value>(102), EXACT, static_cast<Value>(1002));
  const TT::Entry* e2 = tt.getMatch(key2);
  EXPECT_EQ(102, e2->value);

  // new entry in occupied slot
  tt.put(key3, static_cast<Depth>(7), Move(SQ_E2, SQ_E4), static_cast<Value>(103), EXACT, static_cast<Value>(1003));
  const TT::Entry* e3 = tt.getMatch(key3);
  EXPECT_EQ(103, e3->value);

  const TT::Entry* e4 = tt.getMatch(key4);// not in TT
  EXPECT_EQ(nullptr, e4);
}

// 17.6.2020 (loaner laptop)
// Run time      : 976.401.106 ns (102.416.926 put/probes per sec)
TEST_F(TT_Test, TT_PPS) {
  std::random_device rd;
  std::default_random_engine rg1(rd());
  std::uniform_int_distribution<unsigned long long> randomKey(1, 10'000'000);
  std::uniform_int_distribution<unsigned short> randomDepth(0, DEPTH_MAX);
  std::uniform_int_distribution<> randomValue(VALUE_MIN, VALUE_MAX);
  std::uniform_int_distribution<unsigned short> randomType(1, 3);

  TT tt(1024);

  fprintln("Start perft test for TT...");
  fprintln("TT Stats: {:s}", tt.str());

  constexpr Move move(SQ_E2, SQ_E4);
  constexpr int rounds = 5;
  // ReSharper disable once CppTooWideScope
  constexpr int iterations = 100'000'000;

  for (int j = 0; j < rounds; ++j) {
    uint64_t sum     = 0;
    const ZobristKey key    = randomKey(rg1);
    const auto depth = static_cast<Depth>(randomDepth(rg1));
    const auto value = static_cast<Value>(randomValue(rg1));
    const auto type  = static_cast<ValueType>(randomType(rg1));

    auto start = high_resolution_clock::now();
    // puts
    for (int i = 0; i < iterations; ++i) {
      tt.put(key + i, depth, move, value, type, VALUE_NONE);
    }
    // probes
    for (int i = 0; i < iterations; ++i) {
      if (const auto e = tt.probe(key + 2 * i)) {
        const volatile auto v = e->value;
        (void) v;
      }
    }
    auto finish = high_resolution_clock::now();

    sum += std::chrono::duration_cast<nanoseconds>(finish - start).count();
    const double sec = static_cast<double>(sum) / nanoPerSec;
    auto tts         = static_cast<uint64_t>(iterations / sec);
    fprintln("TT Statistics : {:s}", tt.str());
    fprintln("Run time      : {:L} ns ({:L} put/probes per sec)", sum, tts);
    fprintln("");
  }
}

// =============================================================================
// SMP Phase 1 - Thread Safety Stress Test
// Verifies that concurrent put/probe cycles do not crash or produce corrupted
// data visible through a successful probe hit.
//
// What this test DOES verify:
//   - No crash or deadlock under 4-thread concurrent put/probe
//   - When probe() returns non-null (key matched), the fields published by
//     the release-store in put() are coherent: value and depth are within the
//     ranges that any thread ever wrote. A torn write slipping past the key
//     check would produce garbage values outside those ranges.
//
// What this test CANNOT verify:
//   - That UB is truly absent (need TSAN on Linux for that guarantee)
//   - That a hit always returns the most recent write for that key
//     (another thread may overwrite the slot between our put and probe —
//     that is correct Lazy SMP behavior, not a bug)
//
// Run with -fsanitize=thread (TSAN) on WSL/Linux for a definitive race check.
// =============================================================================
TEST_F(TT_Test, ConcurrentPutProbeNoUB) {
  constexpr int NUM_THREADS = 4;
  constexpr int ITERATIONS  = 500'000;
  constexpr int TT_SIZE_MB  = 4; // small = high collision rate = more contention

  TT tt(TT_SIZE_MB);
  tt.setSmpThreads(NUM_THREADS); // disables age-- to avoid bitfield race

  // Depth and value ranges written by all threads — any probe hit must fall within these.
  constexpr int DEPTH_LO = 1;
  constexpr int DEPTH_HI = 20;

  const auto threadWork = [&](const int threadId) {
    std::mt19937_64 rng(static_cast<uint64_t>(threadId) * 0xDEADBEEF12345678ULL);
    std::uniform_int_distribution<ZobristKey> keyDist(1, 1'000'000);
    std::uniform_int_distribution<int> depthDist(DEPTH_LO, DEPTH_HI);
    std::uniform_int_distribution<int> valueDist(VALUE_MIN, VALUE_MAX);

    for (int i = 0; i < ITERATIONS; ++i) {
      const ZobristKey key = keyDist(rng);
      const auto depth     = static_cast<Depth>(depthDist(rng));
      const auto value     = static_cast<Value>(valueDist(rng));
      constexpr Move move(SQ_E2, SQ_E4);

      tt.put(key, depth, move, value, EXACT, VALUE_NONE);

      if (const auto* entry = tt.probe(key)) {
        // probe() returned non-null: the acquire-load on key matched.
        // The release-store in put() that published this key also guaranteed
        // all non-key fields were visible. So depth and value must be values
        // that a legitimate put() wrote — not torn garbage.
        //
        // Note: entry->key may no longer equal 'key' here if another thread
        // overwrote the slot after our acquire-load — that is valid SMP behavior.
        // We check the data fields only, which were coherent at the moment of the hit.
        EXPECT_GE(entry->depth, DEPTH_LO);
        EXPECT_LE(entry->depth, DEPTH_HI);
        EXPECT_GE(entry->value, VALUE_MIN);
        EXPECT_LE(entry->value, VALUE_MAX);
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

  fprintln("ConcurrentPutProbeNoUB: {} threads x {} iterations completed cleanly",
           NUM_THREADS, ITERATIONS);
  fprintln("TT Stats after stress: {}", tt.str());
}
