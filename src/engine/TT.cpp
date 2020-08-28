/*
 * MIT License
 *
 * Copyright (c) 2018-2020 Frank Kopp
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

#include <chrono>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

#include "TT.h"
#include "common/Logging.h"

TT::TT(uint64_t newSizeInMByte) {
  noOfThreads = std::thread::hardware_concurrency();
  resize(newSizeInMByte);
}

void TT::resize(const uint64_t newSizeInMByte) {
  if (newSizeInMByte > MAX_SIZE_MB) {
    LOG__ERROR(Logger::get().TT_LOG, "Requested size for TT of {:L} MB reduced to max of {:L} MB", newSizeInMByte, MAX_SIZE_MB);
    sizeInByte = MAX_SIZE_MB * MB;
  }
  else {
    LOG__TRACE(Logger::get().TT_LOG, "Resizing TT from {:L} MB to {:L} MB", sizeInByte, newSizeInMByte);
    sizeInByte = newSizeInMByte * MB;
  }

  // find the highest power of 2 smaller than maxPossibleEntries
  maxNumberOfEntries = (1ULL << static_cast<uint64_t>(std::floor(std::log2(sizeInByte / ENTRY_SIZE))));
  hashKeyMask        = maxNumberOfEntries - 1;

  // if TT is resized to 0 we cant have any entries.
  if (sizeInByte == 0) maxNumberOfEntries = 0;
  sizeInByte = maxNumberOfEntries * ENTRY_SIZE;

  delete[] _data;
  // try to allocate memory for TT - repeat until allocation is successful
  while (true) {
    try {
      _data = new Entry[maxNumberOfEntries];
      break;
    } catch (std::bad_alloc const&) {
      // we could not allocate enough memory so we reduce TT size by a power of 2
      auto oldSize       = sizeInByte;
      maxNumberOfEntries = maxNumberOfEntries >> 1ULL;
      hashKeyMask        = maxNumberOfEntries - 1;
      sizeInByte         = maxNumberOfEntries * ENTRY_SIZE;
      LOG__ERROR(Logger::get().TT_LOG, "Not enough memory for requested TT size {:L} MB reducing to {:L} MB", oldSize, sizeInByte);
    }
  }

  clear();
  if (maxNumberOfEntries) {
    LOG__INFO(Logger::get().TT_LOG, "TT Size {:L} MByte, Capacity {:L} entries (size={}Byte) (Requested were {:L} MBytes)",
              sizeInByte / MB, maxNumberOfEntries, sizeof(Entry), newSizeInMByte);
  }
}

void TT::clear() {
  if (!maxNumberOfEntries) {
    return;
  }
  // This clears the TT by overwriting each entry with 0.
  // It uses multiple threads if noOfThreads is > 1.
  LOG__TRACE(Logger::get().TT_LOG, "Clearing TT ({} threads)...", noOfThreads);

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
        _data[i].key        = 0;
        _data[i].move       = MOVE_NONE;
        _data[i].depth      = DEPTH_NONE;
        _data[i].value      = VALUE_NONE;
        _data[i].eval       = VALUE_NONE;
        _data[i].age        = 1;
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

  auto finish = std::chrono::high_resolution_clock::now();
  auto time   = std::chrono::duration_cast<std::chrono::milliseconds>(finish - startTime).count();

  LOG__DEBUG(Logger::get().TT_LOG, "TT cleared {:L} entries in {:L} ms ({} threads)", maxNumberOfEntries, time, noOfThreads);
}

void TT::put(Key key, Depth depth, Move move, Value value, ValueType type, Value eval) {

  // if the size of the TT = 0 we
  // do not store anything
  if (!maxNumberOfEntries) return;

  // read the entries for this hash
  Entry* entryDataPtr = getEntryPtr(key);

  numberOfPuts++;

  // New entry
  if (entryDataPtr->key == 0) {
    numberOfEntries++;
    entryDataPtr->key        = key;
    entryDataPtr->move       = static_cast<uint16_t>(move);
    entryDataPtr->depth      = depth;
    entryDataPtr->value      = value;
    entryDataPtr->type       = type;
    entryDataPtr->age        = 1;
    entryDataPtr->eval       = eval;
    return;
  }

  // Same hash but different position
  if (entryDataPtr->key != key) {
    numberOfCollisions++;
    // overwrite if
    // - the new entry's depth is higher
    // - the new entry's depth is same and the previous entry has not been used (is aged)
    if (depth > entryDataPtr->depth ||
        (depth == entryDataPtr->depth && entryDataPtr->age > 0)) {
      numberOfOverwrites++;
      entryDataPtr->key        = key;
      entryDataPtr->move       = static_cast<uint16_t>(move);
      entryDataPtr->depth      = depth;
      entryDataPtr->value      = value;
      entryDataPtr->type       = type;
      entryDataPtr->age        = 1;
      entryDataPtr->eval       = eval;
    }
    return;
  }

  // Same hash and same position -> update entry?
  if (entryDataPtr->key == key) {
    numberOfUpdates++;
    // we always update as the stored moved can't be any good otherwise
    // we would have found this during the search in a previous probe
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

const TT::Entry* TT::probe(const Key& key) {
  numberOfProbes++;

  Entry* ttEntryPtr = getEntryPtr(key);
  if (ttEntryPtr->key == key) {
    numberOfHits++;                        // entries with identical keys found
    if (ttEntryPtr->age) ttEntryPtr->age--;// mark the entry as used
    return ttEntryPtr;
  }

  numberOfMisses++;// keys not found (not equal to TT misses)
  return nullptr;
}

void TT::ageEntries() {
  if (!maxNumberOfEntries) {
    return;
  }
  LOG__TRACE(Logger::get().TT_LOG, "Aging TT ({} threads)...", noOfThreads);
  auto timePoint = std::chrono::high_resolution_clock::now();

  // split work onto multiple threads
  std::vector<std::thread> threads;
  threads.reserve(noOfThreads);
  for (unsigned int idx = 0; idx < noOfThreads; ++idx) {
    threads.emplace_back([&, this, idx]() {
      auto range = maxNumberOfEntries / noOfThreads;
      auto start = idx * range;
      auto end   = start + range;
      if (idx == noOfThreads - 1) end = maxNumberOfEntries;
      for (std::size_t i = start; i < end; ++i) {
        if (_data[i].key == 0) continue;
        _data[i].age++;
        if (_data[i].age > 7) _data[i].age = 7;
      }
    });
  }

  // wait for the threads to finish
  for (std::thread& th : threads) th.join();

  auto finish = std::chrono::high_resolution_clock::now();
  auto time   = std::chrono::duration_cast<std::chrono::milliseconds>(finish - timePoint).count();
  LOG__DEBUG(Logger::get().TT_LOG, "TT aged {:L} entries in {:L} ms ({} threads)", maxNumberOfEntries, time, noOfThreads);
}

std::string TT::str() {
  return fmt::format(
    "TT: size {:L} MB max entries {:L} of size {:L} Bytes entries {:L} ({:L}%) puts {:L} "
    "updates {:L} collisions {:L} overwrites {:L} probes {:L} hits {:L} ({:L}%) misses {:L} ({:L}%)",
    sizeInByte / MB, maxNumberOfEntries, sizeof(Entry), numberOfEntries, hashFull() / 10,
    numberOfPuts, numberOfUpdates, numberOfCollisions, numberOfOverwrites, numberOfProbes,
    numberOfHits, numberOfProbes ? (numberOfHits * 100) / numberOfProbes : 0,
    numberOfMisses, numberOfProbes ? (numberOfMisses * 100) / numberOfProbes : 0);
}

std::ostream& operator<<(std::ostream& os, const TT::Entry& entry) {
  os << "key: " << entry.key << " depth: " << entry.depth << " move: " << entry.move << " value: "
     << entry.value << " type: " << entry.type << " age: " << entry.age;
  return os;
}
