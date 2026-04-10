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
#include <thread>

#include "PawnTT.h"
#include "common/Logging.h"
#include "config/ConfigMode.h"

using namespace engine;
using namespace chess;
using namespace common;

// --- Entry copy constructor/assignment (out-of-line, uses ENTRY_PAYLOAD_SIZE) ---

PawnTT::Entry::Entry(const Entry& other)
    : key(other.key.load(std::memory_order_relaxed)) {
  std::memcpy(&midvalue, &other.midvalue, ENTRY_PAYLOAD_SIZE);
}

PawnTT::Entry& PawnTT::Entry::operator=(const Entry& other) {
  if (this != &other) {
    key.store(other.key.load(std::memory_order_relaxed), std::memory_order_relaxed);
    std::memcpy(&midvalue, &other.midvalue, ENTRY_PAYLOAD_SIZE);
  }
  return *this;
}

PawnTT::PawnTT(const uint64_t newSizeInMByte) {
  noOfThreads = std::max(1U, std::thread::hardware_concurrency());
  resize(newSizeInMByte);
}

void PawnTT::resize(const uint64_t newSizeInMByte) {
  LOG__INFO(Logger::get().EVAL_LOG, "Resizing PawnTT to {:L} MByte ({} threads)", newSizeInMByte, noOfThreads);
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
  std::size_t tryEntries = maxNumberOfEntries ? maxNumberOfEntries : 1U;
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
        tryEntries = maxNumberOfEntries ? maxNumberOfEntries : 1U;
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
  statsSlots = {};

  if (!maxNumberOfEntries) {
    LOG__DEBUG(Logger::get().EVAL_LOG, "PawnTT cleared - no entries");
    return;
  }

  // Clear PawnTT using memset - much faster than parallel for_each.
  LOG__TRACE(Logger::get().EVAL_LOG, "Clearing PawnTT (memset)...");
  const auto startTime = high_resolution_clock::now();

  // Single memset over the entire contiguous array.
  // memset(0) is safe: key==0 marks entries as empty. No code reads other fields
  // from empty entries — put() writes all fields before publishing a non-zero key.
  std::memset(_data.get(), 0, maxNumberOfEntries * sizeof(Entry));

  const auto finish = high_resolution_clock::now();
  const auto time   = std::chrono::duration_cast<milliseconds>(finish - startTime).count();

  LOG__DEBUG(Logger::get().EVAL_LOG, "PawnTT cleared {:L} entries in {:L} ms (memset)", maxNumberOfEntries, time);
}

void PawnTT::put(Entry* entryPtr, const ZobristKey key, const Score score,
                 const Bitboard passedWhite, const Bitboard passedBlack, const int threadIdx) {

  // Replace any existing entries as this should be collisions.
  // Updates should not happen as we should have read this entry and
  // therefore not re-calculated

  TT_STAT_INC(statsSlots[threadIdx].numberOfPuts);

  // Read the current key with relaxed order - we only need its value, not synchronization here.
  // The release store below (when we write a new key) provides the ordering guarantee for readers.
  const ZobristKey entryKey = entryPtr->key.load(std::memory_order_relaxed);

  // New entry
  if (entryKey == 0) {
    TT_STAT_INC(statsSlots[threadIdx].numberOfEntries);
  } // update - should not happen in single-thread mode
  else if (entryKey == key) {
    TT_STAT_INC(statsSlots[threadIdx].numberOfUpdates);
    // Under SMP, concurrent threads may legitimately write the same entry - not a bug.
    // Only warn in single-thread mode where this indicates a missing read optimization.
    if (numSmpThreads <= 1) {
      LOG__WARN(Logger::get().EVAL_LOG, "PawnTT should not have to update entries. Missing a read?");
    }
  }
  else { // collision replaces former entry
    TT_STAT_INC(statsSlots[threadIdx].numberOfCollisions);
  }

  // Write value fields first, then publish via release store on key.
  // Any thread that loads key with acquire will see all prior writes.
  entryPtr->midvalue     = score.midgame;
  entryPtr->endvalue     = score.endgame;
  entryPtr->passedWhite  = passedWhite;
  entryPtr->passedBlack  = passedBlack;
  entryPtr->key.store(key, std::memory_order_release);

  // Statistics counters are non-atomic - under SMP they will be approximate but
  // that is acceptable (diagnostics only, no effect on correctness).
  // Assert is skipped under SMP since counters will not sum correctly.
  assert(numSmpThreads > 1 || statsSlots[threadIdx].numberOfPuts == (statsSlots[threadIdx].numberOfEntries + statsSlots[threadIdx].numberOfCollisions + statsSlots[threadIdx].numberOfUpdates));
}

std::optional<PawnTT::Entry> PawnTT::probe(const ZobristKey key, const int threadIdx) const {
  TT_STAT_INC(statsSlots[threadIdx].numberOfQueries);

  // Key 0 is reserved for empty entries - never matches.
  // This handles positions with no pawns (valid but uncacheable).
  if (key == 0) {
    TT_STAT_INC(statsSlots[threadIdx].numberOfMisses);
    return std::nullopt;
  }

  const Entry* const entryPtr = &_data[getHash(key)];

  // Load key with acquire semantics to synchronize with put()'s release store.
  const ZobristKey storedKey = entryPtr->key.load(std::memory_order_acquire);

  // IMMEDIATELY copy the entry after key load to minimize torn-read window.
  // This copy-on-read pattern prevents races where another thread overwrites
  // the entry between our key check and value reads.
  const Entry copy = *entryPtr; // Copy via copy constructor

  // Verify key match
  if (storedKey == key) {
    TT_STAT_INC(statsSlots[threadIdx].numberOfHits);
    return copy;
  }

  TT_STAT_INC(statsSlots[threadIdx].numberOfMisses);
  return std::nullopt;
}

std::string PawnTT::str() {
  const auto s = aggregateStats();
  return std::format(
    "PawnTT: size {:L} MB max entries {:L} of size {:L} Bytes entries {:L} puts {:L} "
    "updates {:L} collisions {:L} overwrites {:L} hits {:L} ({:L}%) misses {:L} ({:L}%)",
    sizeInByte / MB, maxNumberOfEntries, sizeof(Entry), s.numberOfEntries,
    s.numberOfPuts, s.numberOfUpdates, s.numberOfCollisions, s.numberOfOverwrites,
    s.numberOfHits, s.numberOfQueries ? (s.numberOfHits * 100) / s.numberOfQueries : 0,
    s.numberOfMisses, s.numberOfQueries ? (s.numberOfMisses * 100) / s.numberOfQueries : 0);
}

std::ostream& operator<<(std::ostream& os, const PawnTT::Entry& entry) {
  os << "key: " << entry.key.load(std::memory_order_relaxed) << " midvalue: " << entry.midvalue << " endvalue: " << entry.endvalue;
  return os;
}
