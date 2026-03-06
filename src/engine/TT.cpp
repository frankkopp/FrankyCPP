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

#include <bit>
#include <chrono>
#include <climits>
#include <cstring>
#include <iostream>
#include <mutex>
#include <new>
#include <thread>

#include "TT.h"
#include "common/Logging.h"

#include <algorithm>
#include <execution>

using namespace engine;
using namespace chess;
using namespace common;

std::ostream& operator<<(std::ostream& os, const TT::Entry& entry) {
  os << "key: " << entry.key.load(std::memory_order_relaxed)
     << " depth: " << static_cast<int>(entry.depth)
     << " move: " << entry.move << " value: " << entry.value
     << " type: " << TT::str(entry.type) << " age: " << static_cast<int>(entry.age);
  return os;
}

TT::TT(const uint64_t newSizeInMByte) : noOfThreads(std::thread::hardware_concurrency()) {
  if (noOfThreads == 0) noOfThreads = 1;// ensure at least one thread
  resize(newSizeInMByte);
}

void TT::resize(const uint64_t newSizeInMByte) {
  LOG__INFO(Logger::get().TT_LOG, "Resizing TT to {:L} MByte ({} threads)", newSizeInMByte, noOfThreads);
  if (newSizeInMByte > MAX_SIZE_MB) {
    LOG__ERROR(Logger::get().TT_LOG, "Requested size for TT of {:L} MB reduced to max of {:L} MB", newSizeInMByte, MAX_SIZE_MB);
    sizeInByte = MAX_SIZE_MB * MB;
  }
  else {
    LOG__TRACE(Logger::get().TT_LOG, "Resizing TT from {:L} MB to {:L} MB", sizeInByte / MB, newSizeInMByte);
    sizeInByte = newSizeInMByte * MB;
  }

  // compute capacity in clusters (at least 1 cluster) and derived fields
  uint64_t clustersRequested = sizeInByte / CLUSTER_BYTE_SIZE;
  if (clustersRequested < 1) clustersRequested = 1;
  maxNumberOfClusters = std::bit_floor(clustersRequested);
  if (maxNumberOfClusters < 1) maxNumberOfClusters = 1;

  clusterMask = maxNumberOfClusters - 1;
  sizeInByte  = maxNumberOfClusters * CLUSTER_BYTE_SIZE;

  // release old tt memory
  _data.reset(nullptr);

  // try to allocate memory for TT - repeat until allocation is successful
  while (true) {
    try {
      _data = std::make_unique<TTCluster[]>(maxNumberOfClusters); // NOLINT(*-avoid-c-arrays)
      break;
    } catch (std::bad_alloc const&) {
      // we could not allocate enough memory, so we reduce TT size by a power of 2
      const uint64_t oldSize = sizeInByte;
      if (maxNumberOfClusters <= 1) {
        LOG__CRITICAL(Logger::get().TT_LOG, "Unable to allocate minimal TT of 1 cluster ({} bytes). Out of memory.", sizeof(TTCluster));
        throw;// fatal OOM condition for TT invariant (>=1 cluster)
      }
      maxNumberOfClusters = maxNumberOfClusters >> 1ULL;
      if (maxNumberOfClusters < 1) maxNumberOfClusters = 1;
      clusterMask = maxNumberOfClusters - 1;
      sizeInByte  = maxNumberOfClusters * CLUSTER_BYTE_SIZE;
      LOG__ERROR(Logger::get().TT_LOG, "Not enough memory for requested TT size {:L} MB reducing to {:L} MB", oldSize / MB, sizeInByte / MB);
    }
  }

  clear();
  const std::size_t maxEntries = maxNumberOfClusters * CLUSTER_SIZE;
  LOG__INFO(Logger::get().TT_LOG, "TT Size {:L} MByte, Capacity {:L} entries in {:L} clusters (entry={}B, cluster={}B) (Requested were {:L} MBytes)",
            sizeInByte / MB, maxEntries, maxNumberOfClusters, sizeof(Entry), sizeof(TTCluster), newSizeInMByte);
}

void TT::clear() {
  // Clear TT using a standard parallel algorithm (implementation-defined threading).
  LOG__TRACE(Logger::get().TT_LOG, "Clearing TT (memset)...");
  const auto startTime = high_resolution_clock::now();

  // Single memset over the entire contiguous array.
  // memset(0) is safe: key==0 marks entries as empty. No code reads other fields
  // from empty entries — put() writes all fields before publishing a non-zero key.
  std::memset(_data.get(), 0, maxNumberOfClusters * sizeof(TTCluster));

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

  const std::size_t totalEntries = maxNumberOfClusters * CLUSTER_SIZE;
  LOG__DEBUG(Logger::get().TT_LOG, "TT cleared {:L} entries ({:L} clusters) in {:L} ms (memset)",
             totalEntries, maxNumberOfClusters, time);
}

void TT::put(const ZobristKey key, const Depth depth, const Move move, const Value value, const ValueType type, const Value eval) {

#if TT_USE_MUTEX
  std::lock_guard lock(ttMutex);
#endif

  // get the cluster for this hash
  TTCluster* const cluster = getCluster(key);

  numberOfPuts++;

  // Scan all entries in the cluster for:
  // 1. Exact key match (update existing entry)
  // 2. Empty slot (new entry)
  // 3. Replacement victim (lowest score = weakest)
  Entry* emptyEntry  = nullptr;
  Entry* victimEntry = nullptr;
  int victimScore    = INT_MAX;

  for (auto& entry : cluster->entries) {
    const ZobristKey storedKey = entry.key.load(std::memory_order_relaxed);

    // Same position -> update existing entry
    // Stored key is XOR'd with data hash, so recover original key for comparison.
    if ((storedKey ^ entry.dataHash()) == key) {
      numberOfUpdates++;
      // keep existing move if no move is given
      if (move) {
        entry.move = static_cast<uint16_t>(move);
      }
      // preserve existing value/depth/type if no valid value is given
      if (value != VALUE_NONE) {
        entry.depth = depth;
        entry.value = value;
        entry.type  = type;
        entry.age   = 1;
      }
      // preserve existing eval if no valid value is given
      if (eval != VALUE_NONE) {
        entry.eval = eval;
      }
      // Re-store key XOR'd with updated data hash.
      // Data fields changed, so the stored key must be updated to match.
      entry.key.store(key ^ entry.dataHash(), std::memory_order_release);
      return;
    }

    // Track first empty slot
    if (storedKey == 0 && emptyEntry == nullptr) {
      emptyEntry = &entry;
      continue;
    }

    // Track replacement victim: entry with lowest replacement score.
    // Score formula: depth * 16 - age * 2 + hasMove
    // Higher depth = more valuable, higher age = less valuable, having move = slightly more valuable.
    // Branchless: (move != 0) evaluates to 0 or 1.
    if (storedKey != 0) {
      const int score = static_cast<int>(entry.depth) * 16
                      - static_cast<int>(entry.age) * 2
                      + static_cast<int>(entry.move != 0);
      if (score < victimScore) {
        victimScore = score;
        victimEntry = &entry;
      }
    }
  }

  // No key match found - use empty slot if available
  if (emptyEntry != nullptr) {
    numberOfEntries++;
    // Write non-key fields first, then publish via release store on key.
    // Any thread that loads key with acquire will see all prior writes.
    // XOR key with data hash to detect torn reads in probe().
    emptyEntry->move  = static_cast<uint16_t>(move);
    emptyEntry->depth = depth;
    emptyEntry->value = value;
    emptyEntry->type  = type;
    emptyEntry->age   = 1;
    emptyEntry->eval  = eval;
    emptyEntry->key.store(key ^ emptyEntry->dataHash(), std::memory_order_release);
    return;
  }

  // No empty slot - always replace the weakest victim in the cluster.
  // The 4-way associativity protects valuable entries (3 others survive).
  if (victimEntry != nullptr) {
    numberOfCollisions++;
    numberOfOverwrites++;
    // Write non-key fields first, then publish via release store on key.
    // XOR key with data hash to detect torn reads in probe().
    victimEntry->move  = static_cast<uint16_t>(move);
    victimEntry->depth = depth;
    victimEntry->value = value;
    victimEntry->type  = type;
    victimEntry->age   = 1;
    victimEntry->eval  = eval;
    victimEntry->key.store(key ^ victimEntry->dataHash(), std::memory_order_release);
  }
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::optional<TT::Entry> TT::probe(const ZobristKey& key) {
#if TT_USE_MUTEX
  // used for debugging only
  std::lock_guard lock(ttMutex);
#endif

  numberOfProbes++;
  TTCluster* const cluster = getCluster(key);

  // Scan all entries in the cluster for a key match.
  // XOR verification: storedKey ^ dataHash must equal the original key.
  // If a torn read corrupts any field (key or data), the XOR won't match,
  // causing a clean miss rather than returning corrupted data.
  for (auto& entry : cluster->entries) {
    const ZobristKey storedKey = entry.key.load(std::memory_order_acquire);

    // IMMEDIATELY copy the entry after key load to minimize torn-read window.
    // We copy before verification because dataHash() reads the data fields.
    Entry copy = entry;// Copy via copy constructor

    // Verify: storedKey XOR'd with data hash must equal the probe key.
    // This detects torn reads: if any field was corrupted, the reconstructed
    // key won't match, and we treat it as a miss (safe fallback).
    if ((storedKey ^ copy.dataHash()) == key) {
      // Restore the original key in the copy (it was stored XOR'd)
      copy.key.store(key, std::memory_order_relaxed);
      numberOfHits++;

      // age-- marks the entry as recently used, making it less likely to be evicted
      // by the replacement policy in put(). Safe in single-thread mode only.
      // Skipped under SMP (numSmpThreads > 1): age is a bitfield packed in the same
      // byte as depth/type; a concurrent put() writing that byte is a data race.
      if (numSmpThreads <= 1) {
        if (entry.age > 0) {
          entry.age--;
          // Re-store key XOR'd with updated data hash after modifying age
          entry.key.store(key ^ entry.dataHash(), std::memory_order_release);
        }
      }

      return copy;
    }
  }
  numberOfMisses++;
  return std::nullopt;
}

void TT::ageEntries() {
  LOG__TRACE(Logger::get().TT_LOG, "Aging TT (std::execution::par_unseq)...");
  const auto timePoint = high_resolution_clock::now();

  std::for_each(
    std::execution::par_unseq,
    _data.get(),
    _data.get() + maxNumberOfClusters,
    [](TTCluster& cluster) {
      for (auto& e : cluster.entries) {
        const ZobristKey storedKey = e.key.load(std::memory_order_relaxed);
        if (storedKey == 0) continue;
        // Recover original key before modifying data
        const ZobristKey originalKey = storedKey ^ e.dataHash();
        if (e.age < 7) e.age++;
        // Re-store key XOR'd with updated data hash
        e.key.store(originalKey ^ e.dataHash(), std::memory_order_relaxed);
      }
    });

  const auto finish = high_resolution_clock::now();
  const auto time   = std::chrono::duration_cast<milliseconds>(finish - timePoint).count();
  (void) time;
  const std::size_t totalEntries = maxNumberOfClusters * CLUSTER_SIZE;
  LOG__DEBUG(Logger::get().TT_LOG, "TT aged {:L} entries ({:L} clusters) in {:L} ms (policy=par_unseq)",
             totalEntries, maxNumberOfClusters, time);
}

std::string TT::str() const {
  const std::size_t maxEntries = maxNumberOfClusters * CLUSTER_SIZE;
  return std::format(
    deLocale,
    "TT: size {:L} MB max entries {:L} ({:L} clusters x {}) of size {:L} Bytes entries {:L} ({:L}%) puts {:L} "
    "updates {:L} collisions {:L} overwrites {:L} probes {:L} hits {:L} ({:L}%) misses {:L} ({:L}%)",
    sizeInByte / MB, maxEntries, maxNumberOfClusters, CLUSTER_SIZE, sizeof(Entry), numberOfEntries, hashFull() / 10,
    numberOfPuts, numberOfUpdates, numberOfCollisions, numberOfOverwrites, numberOfProbes,
    numberOfHits, numberOfProbes ? (numberOfHits * 100) / numberOfProbes : 0,
    numberOfMisses, numberOfProbes ? (numberOfMisses * 100) / numberOfProbes : 0);
}
