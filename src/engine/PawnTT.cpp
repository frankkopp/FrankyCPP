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

#include <algorithm>
#include <bit>
#include <cmath>
#include <execution>
#include <thread>

#include "PawnTT.h"
#include "common/Logging.h"

using namespace engine;
using namespace chess;
using namespace common;

PawnTT::PawnTT(const uint64_t newSizeInMByte) {
  noOfThreads = std::max(1u, std::thread::hardware_concurrency());
  resize(newSizeInMByte);
}

void PawnTT::resize(const uint64_t newSizeInMByte) {
  if (newSizeInMByte > MAX_SIZE_MB) {
    LOG__ERROR(Logger::get().EVAL_LOG, "Requested size for PawnTT of {:L} MB reduced to max of {:L} MB", newSizeInMByte, MAX_SIZE_MB);
    sizeInByte = MAX_SIZE_MB * MB;
  }
  else {
    LOG__TRACE(Logger::get().EVAL_LOG, "Resizing PawnTT from {:L} MB to {:L} MB", sizeInByte / MB, newSizeInMByte);
    sizeInByte = newSizeInMByte * MB;
  }

  // if PawnTT is resized to 0 we cant have any entries.
  if (sizeInByte == 0) {
    maxNumberOfEntries = 0;
  }
  else {
    // find the highest power of 2 smaller than maxPossibleEntries
#if __cpp_lib_int_pow2 >= 202002L
    maxNumberOfEntries = std::bit_floor(sizeInByte / ENTRY_SIZE);
#else
    maxNumberOfEntries = (1ULL << static_cast<uint64_t>(std::floor(std::log2(sizeInByte / ENTRY_SIZE))));
#endif
  }

  // Even when logically disabled (maxNumberOfEntries==0), allocate a single dummy entry
  // so hot-path functions can remain branchless and safely index _data[0].
  hashKeyMask = maxNumberOfEntries ? (maxNumberOfEntries - 1) : 0;
  sizeInByte  = maxNumberOfEntries * ENTRY_SIZE;

  // release old tt memory
  _data.reset(nullptr);

  // try to allocate memory for TT - reduce on failure until success (down to dummy)
  std::size_t tryEntries = maxNumberOfEntries ? maxNumberOfEntries : 1u;
  while (true) {
    try {
      _data = std::make_unique<Entry[]>(tryEntries);
      break;
    } catch (std::bad_alloc const&) {
      if (maxNumberOfEntries) {
        const uint64_t oldSizeMb = sizeInByte / MB;
        maxNumberOfEntries >>= 1ULL;
        hashKeyMask = maxNumberOfEntries ? (maxNumberOfEntries - 1) : 0;
        sizeInByte  = maxNumberOfEntries * ENTRY_SIZE;
        LOG__ERROR(Logger::get().EVAL_LOG,
                   "Not enough memory for requested PawnTT size {:L} MB reducing to {:L} MB",
                   oldSizeMb, sizeInByte / MB);
        tryEntries = maxNumberOfEntries ? maxNumberOfEntries : 1u;
        continue;
      }
      // already at dummy size -> last resort
      tryEntries = 1U;
      _data      = std::make_unique<Entry[]>(tryEntries);
      break;
    }
  }

  clear();
  LOG__INFO(Logger::get().EVAL_LOG, "PawnTT Size {:L} MByte, Capacity {:L} entries (size={}Byte) (Requested were {:L} MBytes)",
            sizeInByte / MB, maxNumberOfEntries, sizeof(Entry), newSizeInMByte);
}


void PawnTT::clear() {
  // reset statistics (also when table is logically disabled)
  numberOfEntries    = 0;
  numberOfHits       = 0;
  numberOfUpdates    = 0;
  numberOfMisses     = 0;
  numberOfPuts       = 0;
  numberOfCollisions = 0;
  numberOfOverwrites = 0;
  numberOfQueries    = 0;

  if (!maxNumberOfEntries) {
    LOG__DEBUG(Logger::get().EVAL_LOG, "PawnTT cleared - no entries");
    return;
  }

  LOG__TRACE(Logger::get().EVAL_LOG, "Clearing PawnTT (parallel algorithms)...");
  const auto startTime = high_resolution_clock::now();

  Entry* beginPtr = _data.get();
  Entry* endPtr   = beginPtr + maxNumberOfEntries;

  std::for_each(std::execution::par, beginPtr, endPtr, [](Entry& e) {
    e.key.store(0, std::memory_order_relaxed);
    e.midvalue = VALUE_NONE;
    e.endvalue = VALUE_NONE;
  });

  const auto finish = high_resolution_clock::now();
  const auto time   = std::chrono::duration_cast<milliseconds>(finish - startTime).count();

  LOG__DEBUG(Logger::get().EVAL_LOG, "PawnTT cleared {:L} entries in {:L} ms", maxNumberOfEntries, time);
}

void PawnTT::put(Entry* entryPtr, const ZobristKey key, const Score score) {

  // Replace any existing entries as this should be collisions.
  // Updates should not happen as we should have read this entry and
  // therefore not re-calculated

  numberOfPuts++;

  // Read the current key with relaxed order - we only need its value, not synchronization here.
  // The release store below (when we write a new key) provides the ordering guarantee for readers.
  const ZobristKey entryKey = entryPtr->key.load(std::memory_order_relaxed);

  // New entry
  if (entryKey == 0) {
    numberOfEntries++;
  }// update - should not happen in single-thread mode
  else if (entryKey == key) {
    numberOfUpdates++;
    // Under SMP, concurrent threads may legitimately write the same entry - not a bug.
    // Only warn in single-thread mode where this indicates a missing read optimization.
    if (numSmpThreads <= 1) {
      LOG__WARN(Logger::get().EVAL_LOG, "PawnTT should not have to update entries. Missing a read?");
    }
  }
  else {// collision replaces former entry
    numberOfCollisions++;
  }

  // Write value fields first, then publish via release store on key.
  // Any thread that loads key with acquire will see all prior writes.
  entryPtr->midvalue = score.midgame;
  entryPtr->endvalue = score.endgame;
  entryPtr->key.store(key, std::memory_order_release);

  // Statistics counters are non-atomic - under SMP they will be approximate but
  // that is acceptable (diagnostics only, no effect on correctness).
  // Assert is skipped under SMP since counters will not sum correctly.
  assert(numSmpThreads > 1 || numberOfPuts == (numberOfEntries + numberOfCollisions + numberOfUpdates));
}

std::string PawnTT::str() {
  return std::format(
    "PawnTT: size {:L} MB max entries {:L} of size {:L} Bytes entries {:L} puts {:L} "
    "updates {:L} collisions {:L} overwrites {:L} hits {:L} ({:L}%) misses {:L} ({:L}%)",
    sizeInByte / MB, maxNumberOfEntries, sizeof(Entry), numberOfEntries,
    numberOfPuts, numberOfUpdates, numberOfCollisions, numberOfOverwrites,
    numberOfHits, numberOfQueries ? (numberOfHits * 100) / numberOfQueries : 0,
    numberOfMisses, numberOfQueries ? (numberOfMisses * 100) / numberOfQueries : 0);
}

std::ostream& operator<<(std::ostream& os, const PawnTT::Entry& entry) {
  os << "key: " << entry.key.load(std::memory_order_relaxed) << " midvalue: " << entry.midvalue << " endvalue: " << entry.endvalue;
  return os;
}
