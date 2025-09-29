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
#include <chrono>
#include <iostream>
#include <new>
#include <thread>

#include "TT.h"
#include "common/Logging.h"

#include <execution>
#include <algorithm>

std::ostream& operator<<(std::ostream& os, const TT::Entry& entry) {
  os << "key: " << entry.key << " depth: " << static_cast<int>(entry.depth)
     << " move: " << entry.move << " value: " << entry.value
     << " type: " << TT::str(entry.type) << " age: " << static_cast<int>(entry.age);
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
 // Clear TT using a standard parallel algorithm (implementation-defined threading).
 LOG__TRACE(Logger::get().TT_LOG, "Clearing TT (std::execution::par_unseq)...");
 const auto startTime = high_resolution_clock::now();

 std::for_each(
   std::execution::par_unseq,
   _data.get(),
   _data.get() + maxNumberOfEntries,
   [](Entry& e) {
     e.key   = 0;
     e.move  = 0; // MOVE_NONE as 16-bit
     e.depth = DEPTH_NONE;
     e.value = VALUE_NONE;
     e.eval  = VALUE_NONE;
     e.age   = 1;
   }
 );

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

  LOG__DEBUG(Logger::get().TT_LOG, "TT cleared {:L} entries in {:L} ms (policy=par_unseq)", maxNumberOfEntries, time);
}

void TT::put(const ZobristKey key, const Depth depth, const Move move, const Value value, const ValueType type, const Value eval) {

  // read the entries for this hash
  Entry* entry = getEntryPtr(key);

  numberOfPuts++;

  // New entry slot
  if (entry->key == 0) {
    numberOfEntries++;
    entry->key   = key;
    entry->move  = static_cast<uint16_t>(move);
    entry->depth = depth;
    entry->value = value;
    entry->type  = type;
    entry->age   = 1;
    entry->eval  = eval;
    return;
  }

  // Different position colliding in the same slot
  if (entry->key != key) {
    numberOfCollisions++;
    // overwrite if
    // - the new entry's depth is higher
    // - the new entry's depth is same and the previous entry has not been used (is aged)
    if (depth > entry->depth || (depth == entry->depth && entry->age > 0)) {
      numberOfOverwrites++;
      entry->key   = key;
      entry->move  = static_cast<uint16_t>(move);
      entry->depth = depth;
      entry->value = value;
      entry->type  = type;
      entry->age   = 1;
      entry->eval  = eval;
    }
    return;
  }

  // Same position -> update existing entry
  numberOfUpdates++;
  // keep existing move if no move is given
  if (move) {
    entry->move = static_cast<uint16_t>(move);
  }
  // preserve existing value/depth/type if no valid value is given
  if (value != VALUE_NONE) {
    entry->depth = depth;
    entry->value = value;
    entry->type  = type;
    entry->age   = 1;
  }
  // preserve existing eval if no valid value is given
  if (eval != VALUE_NONE) {
    entry->eval = eval;
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
  LOG__TRACE(Logger::get().TT_LOG, "Aging TT (std::execution::par_unseq)...");
  const auto timePoint = high_resolution_clock::now();

  std::for_each(
    std::execution::par_unseq,
    _data.get(),
    _data.get() + maxNumberOfEntries,
    [](Entry& e) {
      if (e.key == 0) return;
      if (e.age < 7) e.age++;
    }
  );

  const auto finish = high_resolution_clock::now();
  const auto time   = std::chrono::duration_cast<milliseconds>(finish - timePoint).count();
  (void) time;
  LOG__DEBUG(Logger::get().TT_LOG, "TT aged {:L} entries in {:L} ms (policy=par_unseq)", maxNumberOfEntries, time);
}

std::string TT::str() const {
  return std::format(
    deLocale,
    "TT: size {:L} MB max entries {:L} of size {:L} Bytes entries {:L} ({:L}%) puts {:L} "
    "updates {:L} collisions {:L} overwrites {:L} probes {:L} hits {:L} ({:L}%) misses {:L} ({:L}%)",
    sizeInByte / MB, maxNumberOfEntries, sizeof(Entry), numberOfEntries, hashFull() / 10,
    numberOfPuts, numberOfUpdates, numberOfCollisions, numberOfOverwrites, numberOfProbes,
    numberOfHits, numberOfProbes ? (numberOfHits * 100) / numberOfProbes : 0,
    numberOfMisses, numberOfProbes ? (numberOfMisses * 100) / numberOfProbes : 0);
}
