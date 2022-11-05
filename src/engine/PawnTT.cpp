// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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

#include <bit>
#include <thread>

#include "PawnTT.h"
#include "common/Logging.h"

PawnTT::PawnTT(uint64_t newSizeInMByte) {
  noOfThreads = std::thread::hardware_concurrency();
  resize(newSizeInMByte);
}

void PawnTT::resize(const uint64_t newSizeInMByte) {
  if (newSizeInMByte > MAX_SIZE_MB) {
    LOG__ERROR(Logger::get().EVAL_LOG, "Requested size for PawnTT of {:L} MB reduced to max of {:L} MB", newSizeInMByte, MAX_SIZE_MB);
    sizeInByte = MAX_SIZE_MB * MB;
  }
  else {
    LOG__TRACE(Logger::get().EVAL_LOG, "Resizing PawnTT from {:L} MB to {:L} MB", sizeInByte, newSizeInMByte);
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

  hashKeyMask = maxNumberOfEntries - 1;
  sizeInByte  = maxNumberOfEntries * ENTRY_SIZE;

  // release old tt memory
  _data.reset(nullptr);

  // try to allocate memory for TT - repeat until allocation is successful
  while (true) {
    try {
      _data = std::make_unique<Entry[]>(maxNumberOfEntries);
      break;
    } catch (std::bad_alloc const&) {
      // we could not allocate enough memory, so we reduce PawnTT size by a power of 2
      uint64_t oldSize   = sizeInByte;
      maxNumberOfEntries = maxNumberOfEntries >> 1ULL;
      hashKeyMask        = maxNumberOfEntries - 1;
      sizeInByte         = maxNumberOfEntries * ENTRY_SIZE;
      LOG__ERROR(Logger::get().EVAL_LOG, "Not enough memory for requested PawnTT size {:L} MB reducing to {:L} MB", oldSize, sizeInByte);
    }
  }

  clear();
  LOG__INFO(Logger::get().EVAL_LOG, "PawnTT Size {:L} MByte, Capacity {:L} entries (size={}Byte) (Requested were {:L} MBytes)",
            sizeInByte / MB, maxNumberOfEntries, sizeof(Entry), newSizeInMByte);
}


void PawnTT::clear() {
  if (!maxNumberOfEntries) {
    LOG__DEBUG(Logger::get().EVAL_LOG, "PawnTT cleared - no entries");
    return;
  }
  // This clears the PawnTT by overwriting each entry with 0.
  // It uses multiple threads if noOfThreads is > 1.
  LOG__TRACE(Logger::get().EVAL_LOG, "Clearing PawnTT ({} threads)...", noOfThreads);

  auto startTime = std::chrono::high_resolution_clock::now();
  std::vector<std::thread> threads;
  threads.reserve(noOfThreads);

  // split work onto multiple threads
  for (unsigned int t = 0; t < noOfThreads; ++t) {
    threads.emplace_back([&, this, t]() {
      auto range = maxNumberOfEntries / noOfThreads;
      auto start = t * range;
      auto end   = start + range;
      if (t == noOfThreads - 1) end = maxNumberOfEntries;
      for (std::size_t i = start; i < end; ++i) {
        _data[i].key      = 0;
        _data[i].midvalue = VALUE_NONE;
        _data[i].endvalue = VALUE_NONE;
      }
    });
  }

  // wait until all threads have finished their work
  for (std::thread& th : threads) th.join();

  // reset statistics
  numberOfEntries = 0;
  numberOfHits    = 0;
  numberOfUpdates = 0;
  numberOfMisses  = 0;

  auto finish = std::chrono::high_resolution_clock::now();
  auto time   = std::chrono::duration_cast<std::chrono::milliseconds>(finish - startTime).count();

  LOG__DEBUG(Logger::get().EVAL_LOG, "PawnTT cleared {:L} entries in {:L} ms ({} threads)", maxNumberOfEntries, time, noOfThreads);
}

void PawnTT::put(Entry* entryDataPtr, Key key, Score score) {

  // Replace any existing entries as this should be collisions.
  // Updates should not happen as we should have read this entry and
  // therefore not re-calculated

  numberOfPuts++;

  // New entry
  if (entryDataPtr->key == 0) {
    numberOfEntries++;
  }// update - should not happen
  else if (entryDataPtr->key == key) {
    numberOfUpdates++;
    LOG__WARN(Logger::get().EVAL_LOG, "PawnTT should not have to update entries. Missing a read?");
  }
  else {// collision replaces former entry
    numberOfCollisions++;
  }

  entryDataPtr->key      = key;
  entryDataPtr->midvalue = score.midgame;
  entryDataPtr->endvalue = score.endgame;

  assert(numberOfPuts == (numberOfEntries + numberOfCollisions + numberOfUpdates));
}

std::string PawnTT::str() {
  return fmt::format(
    "PawnTT: size {:L} MB max entries {:L} of size {:L} Bytes entries {:L} puts {:L} "
    "updates {:L} collisions {:L} overwrites {:L} hits {:L} ({:L}%) misses {:L} ({:L}%)",
    sizeInByte / MB, maxNumberOfEntries, sizeof(Entry), numberOfEntries,
    numberOfPuts, numberOfUpdates, numberOfCollisions, numberOfOverwrites,
    numberOfHits, numberOfQueries ? (numberOfHits * 100) / numberOfQueries : 0,
    numberOfMisses, numberOfQueries ? (numberOfMisses * 100) / numberOfQueries : 0);
}

std::ostream& operator<<(std::ostream& os, const PawnTT::Entry& entry) {
  os << "key: " << entry.key << " midvalue: " << entry.midvalue << " endvalue: " << entry.endvalue;
  return os;
}
