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

#include <chrono>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

#include "TT.h"
#include "common/Logging.h"

std::ostream& operator<<(std::ostream& os, const TT::Entry& entry) {
  os << "key: " << entry.key << " depth: " << entry.depth << " move: " << entry.move << " value: "
     << entry.value << " type: " << entry.type << " age: " << entry.age;
  return os;
}

TT::TT(const uint64_t newSizeInMByte) {
  noOfThreads = std::thread::hardware_concurrency();
  if (noOfThreads == 0) noOfThreads = 1; // ensure at least one thread
  resize(newSizeInMByte);
}

void TT::resize(const uint64_t newSizeInMByte) {
  if (newSizeInMByte > MAX_SIZE_MB) {
    LOG__ERROR(Logger::get().TT_LOG, "Requested size for TT of {:L} MB reduced to max of {:L} MB", newSizeInMByte, MAX_SIZE_MB);
    sizeInByte = MAX_SIZE_MB * MB;
  }
  else {
    LOG__TRACE(Logger::get().TT_LOG, "Resizing TT from {:L} MB to {:L} MB", sizeInByte / MB, newSizeInMByte);
    sizeInByte = newSizeInMByte * MB;
  }

  // compute capacity (at least 1 entry) and derived fields
  uint64_t entriesRequested = sizeInByte / ENTRY_SIZE;
  if (entriesRequested < 1) entriesRequested = 1;
  maxNumberOfEntries = std::bit_floor(entriesRequested);
  if (maxNumberOfEntries < 1) maxNumberOfEntries = 1;

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
      // we could not allocate enough memory, so we reduce TT size by a power of 2
      const uint64_t oldSize   = sizeInByte;
      if (maxNumberOfEntries <= 1) {
        LOG__CRITICAL(Logger::get().TT_LOG, "Unable to allocate minimal TT of 1 entry ({} bytes). Out of memory.", sizeof(Entry));
        throw; // fatal OOM condition for TT invariant (>=1 entry)
      }
      maxNumberOfEntries = maxNumberOfEntries >> 1ULL;
      if (maxNumberOfEntries < 1) maxNumberOfEntries = 1;
      hashKeyMask = maxNumberOfEntries - 1;
      sizeInByte  = maxNumberOfEntries * ENTRY_SIZE;
      LOG__ERROR(Logger::get().TT_LOG, "Not enough memory for requested TT size {:L} MB reducing to {:L} MB", oldSize / MB, sizeInByte / MB);
    }
  }

  clear();
  LOG__INFO(Logger::get().TT_LOG, "TT Size {:L} MByte, Capacity {:L} entries (size={}Byte) (Requested were {:L} MBytes)",
            sizeInByte / MB, maxNumberOfEntries, sizeof(Entry), newSizeInMByte);
}

void TT::clear() {
  // This clears the TT by overwriting each entry with 0.
  // It uses multiple threads if noOfThreads is > 1.
  unsigned int threadsToUse = noOfThreads == 0 ? 1u : noOfThreads;
  if (threadsToUse > maxNumberOfEntries) threadsToUse = static_cast<unsigned int>(maxNumberOfEntries);
  if (threadsToUse == 0) threadsToUse = 1; // defensive
  LOG__TRACE(Logger::get().TT_LOG, "Clearing TT ({} threads)...", threadsToUse);

  const auto startTime = high_resolution_clock::now();
  std::vector<std::thread> threads;
  threads.reserve(threadsToUse);

  // split work onto multiple threads
  for (unsigned int t = 0; t < threadsToUse; ++t) {
    threads.emplace_back([&, this, t]() {
      const auto range = maxNumberOfEntries / threadsToUse;
      const auto start = t * range;
      auto end         = start + range;
      if (t == threadsToUse - 1) end = maxNumberOfEntries;
      for (std::size_t i = start; i < end; ++i) {
        _data[i].key   = 0;
        _data[i].move  = 0;// MOVE_NONE as 16-bit
        _data[i].depth = DEPTH_NONE;
        _data[i].value = VALUE_NONE;
        _data[i].eval  = VALUE_NONE;
        _data[i].age   = 1;
      }
    });
  }

  // wait until all threads have finished their work
  for (std::thread& th : threads) th.join();

  // reset statistics
  numberOfPuts       = 0;
  numberOfEntries    = 0;
  numberOfHits       = 0;
  numberOfUpdates    = 0;
  numberOfMisses     = 0;
  numberOfCollisions = 0;
  numberOfOverwrites = 0;
  numberOfProbes     = 0;

  const auto finish = high_resolution_clock::now();
  const auto time   = std::chrono::duration_cast<milliseconds>(finish - startTime).count();
  (void) time;

  LOG__DEBUG(Logger::get().TT_LOG, "TT cleared {:L} entries in {:L} ms ({} threads)", maxNumberOfEntries, time, threadsToUse);
}

void TT::put(const ZobristKey key, const Depth depth, const Move move, const Value value, const ValueType type, const Value eval) {

  // read the entries for this hash
  Entry* entryDataPtr = getEntryPtr(key);

  numberOfPuts++;

  // New entry
  if (entryDataPtr->key == 0) {
    numberOfEntries++;
    entryDataPtr->key   = key;
    entryDataPtr->move  = static_cast<uint16_t>(move);
    entryDataPtr->depth = depth;
    entryDataPtr->value = value;
    entryDataPtr->type  = type;
    entryDataPtr->age   = 1;
    entryDataPtr->eval  = eval;
    return;
  }

  // Same hash but different position
  if (entryDataPtr->key != key) {
    numberOfCollisions++;
    // overwrite if
    // - the new entry's depth is higher
    // - the new entry's depth is same and the previous entry has not been used (is aged)
    if (depth > entryDataPtr->depth || (depth == entryDataPtr->depth && entryDataPtr->age > 0)) {
      numberOfOverwrites++;
      entryDataPtr->key   = key;
      entryDataPtr->move  = static_cast<uint16_t>(move);
      entryDataPtr->depth = depth;
      entryDataPtr->value = value;
      entryDataPtr->type  = type;
      entryDataPtr->age   = 1;
      entryDataPtr->eval  = eval;
    }
    return;
  }

  // Same hash and same position -> update entry?
  if (entryDataPtr->key == key) {
    numberOfUpdates++;
    // we always update as the stored moved can't be any good otherwise
    // we would have found this during the search in a previous probe,
    // and we would not have come to store it again
    entryDataPtr->key = key;
    if (move) {// preserve existing move if no move is given
      entryDataPtr->move = static_cast<uint16_t>(move);
    }
    if (value != VALUE_NONE) {// preserve existing entry if no valid value is given
      entryDataPtr->depth = depth;
      entryDataPtr->value = value;
      entryDataPtr->type  = type;
      entryDataPtr->age   = 1;
    }
    if (eval != VALUE_NONE) {// preserve existing entry if no valid value is given
      entryDataPtr->eval = eval;
    }
    return;
  }

  assert(numberOfPuts == (numberOfEntries + numberOfCollisions + numberOfUpdates));
}

// ReSharper disable once CppMemberFunctionMayBeConst
const TT::Entry* TT::probe(const ZobristKey& key) {
  numberOfProbes++;
  Entry* ttEntryPtr = getEntryPtr(key);
  if (ttEntryPtr->key == key) {
    numberOfHits++;// entries with identical keys found
    if (ttEntryPtr->age)
      ttEntryPtr->age--;// mark the entry as used
    return ttEntryPtr;
  }
  numberOfMisses++;// keys not found (not equal to TT misses)
  return nullptr;
}

void TT::ageEntries() {
  unsigned int threadsToUse = noOfThreads == 0 ? 1u : noOfThreads;
  if (threadsToUse > maxNumberOfEntries) threadsToUse = static_cast<unsigned int>(maxNumberOfEntries);
  if (threadsToUse == 0) threadsToUse = 1;
  LOG__TRACE(Logger::get().TT_LOG, "Aging TT ({} threads)...", threadsToUse);
  const auto timePoint = high_resolution_clock::now();

  // split work onto multiple threads
  std::vector<std::thread> threads;
  threads.reserve(threadsToUse);
  for (unsigned int idx = 0; idx < threadsToUse; ++idx) {
    threads.emplace_back([&, this, idx]() {
      const auto range = maxNumberOfEntries / threadsToUse;
      const auto start = idx * range;
      auto end         = start + range;
      if (idx == threadsToUse - 1) end = maxNumberOfEntries;
      for (std::size_t i = start; i < end; ++i) {
        if (_data[i].key == 0) continue;
        _data[i].age++;
        if (_data[i].age > 7) _data[i].age = 7;
      }
    });
  }

  // wait for the threads to finish
  for (std::thread& th : threads) th.join();

  const auto finish = high_resolution_clock::now();
  const auto time   = std::chrono::duration_cast<milliseconds>(finish - timePoint).count();
  (void) time;
  LOG__DEBUG(Logger::get().TT_LOG, "TT aged {:L} entries in {:L} ms ({} threads)", maxNumberOfEntries, time, threadsToUse);
}

std::string TT::str() {
  return std::format(
    deLocale,
    "TT: size {:L} MB max entries {:L} of size {:L} Bytes entries {:L} ({:L}%) puts {:L} "
    "updates {:L} collisions {:L} overwrites {:L} probes {:L} hits {:L} ({:L}%) misses {:L} ({:L}%)",
    sizeInByte / MB, maxNumberOfEntries, sizeof(Entry), numberOfEntries, hashFull() / 10,
    numberOfPuts, numberOfUpdates, numberOfCollisions, numberOfOverwrites, numberOfProbes,
    numberOfHits, numberOfProbes ? (numberOfHits * 100) / numberOfProbes : 0,
    numberOfMisses, numberOfProbes ? (numberOfMisses * 100) / numberOfProbes : 0);
}
