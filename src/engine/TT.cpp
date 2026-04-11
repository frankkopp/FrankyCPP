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
#include <mutex>
#include <new>
#include <thread>

#include "TT.h"
#include "common/Logging.h"
#include "config/ConfigMode.h"

using namespace engine;
using namespace chess;
using namespace common;

std::ostream& operator<<(std::ostream& os, const TT::Entry& entry) {
  os << "key: " << entry.key.load(std::memory_order_relaxed)
     << " depth: " << static_cast<int>(entry.depth)
     << " move: " << entry.move << " value: " << entry.value
     << " type: " << TT::str(entry.type) << " gen: " << static_cast<int>(entry.gen);
  return os;
}

TT::TT(const uint64_t newSizeInMByte) : noOfThreads(std::thread::hardware_concurrency()) {
  if (noOfThreads == 0) noOfThreads = 1; // ensure at least one thread
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
        throw; // fatal OOM condition for TT invariant (>=1 cluster)
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
  statsSlots = {};

  // reset generation counter — all entries are zeroed (gen=0), so generation
  // must also be 0 to avoid treating fresh entries as stale after clear().
  generation = 0;

  const auto finish = high_resolution_clock::now();
  const auto time   = std::chrono::duration_cast<milliseconds>(finish - startTime).count();
  (void) time;

  const std::size_t totalEntries = maxNumberOfClusters * CLUSTER_SIZE;
  LOG__DEBUG(Logger::get().TT_LOG, "TT cleared {:L} entries ({:L} clusters) in {:L} ms (memset)",
             totalEntries, maxNumberOfClusters, time);
}

void TT::put(const ZobristKey key, const Depth depth, const Move move, const Value value, const ValueType type, const Value eval, const int threadIdx) {

#if TT_USE_MUTEX
  std::lock_guard lock(ttMutex);
#endif

  // get the cluster for this hash
  TTCluster* const cluster = getCluster(key);

  TT_STAT_INC(statsSlots[threadIdx].numberOfPuts);

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
      TT_STAT_INC(statsSlots[threadIdx].numberOfUpdates);
      // keep existing move if no move is given
      if (move) {
        entry.move = static_cast<uint16_t>(move);
      }
      // preserve existing value/depth/type if no valid value is given
      if (value != VALUE_NONE) {
        entry.depth = depth;
        entry.value = value;
        entry.type  = type;
        entry.gen   = generation;
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
    // Score formula: depth * 16 - staleness * 2 + hasMove
    // where staleness = (generation - entry.gen) & 7 (0 = current gen, 7 = oldest).
    // Higher depth = more valuable, higher staleness = less valuable, having move = slightly more valuable.
    // Branchless: (move != 0) evaluates to 0 or 1.
    if (storedKey != 0) {
      const int staleness = (generation - entry.gen) & 7;
      const int score = static_cast<int>(entry.depth) * 16
                        - staleness * 2
                        + static_cast<int>(entry.move != 0);
      if (score < victimScore) {
        victimScore = score;
        victimEntry = &entry;
      }
    }
  }

  // No key match found - use empty slot if available
  if (emptyEntry != nullptr) {
    TT_STAT_INC(statsSlots[threadIdx].numberOfEntries);
    // Write non-key fields first, then publish via release store on key.
    // Any thread that loads key with acquire will see all prior writes.
    // XOR key with data hash to detect torn reads in probe().
    emptyEntry->move  = static_cast<uint16_t>(move);
    emptyEntry->depth = depth;
    emptyEntry->value = value;
    emptyEntry->type  = type;
    emptyEntry->gen   = generation;
    emptyEntry->eval  = eval;
    emptyEntry->key.store(key ^ emptyEntry->dataHash(), std::memory_order_release);
    return;
  }

  // No empty slot - always replace the weakest victim in the cluster.
  // The 4-way associativity protects valuable entries (3 others survive).
  if (victimEntry != nullptr) {
    TT_STAT_INC(statsSlots[threadIdx].numberOfCollisions);
    TT_STAT_INC(statsSlots[threadIdx].numberOfOverwrites);

    // R6 instrumentation: track replacement depth quality
    if constexpr (TT_INSTRUMENTATION) {
      // ReSharper disable once CppDFAUnreachableCode
      const int oldDepth = victimEntry->depth;
      const int newDepth = depth;
      const int delta    = newDepth - oldDepth;
      instrSlots[threadIdx].replaceDepthDeltaSum += delta;
      instrSlots[threadIdx].replaceVictimDepthSum += static_cast<uint64_t>(oldDepth);
      if (delta < 0)       { instrSlots[threadIdx].replaceDepthDown++; }
      else if (delta > 0)  { instrSlots[threadIdx].replaceDepthUp++; }
      else                 { instrSlots[threadIdx].replaceDepthEqual++; }
      // Track deep-entry evictions separately (victim.depth >= threshold)
      if (oldDepth >= DEEP_ENTRY_THRESHOLD) {
        instrSlots[threadIdx].replaceDeepTotal++;
        if (delta < 0) { instrSlots[threadIdx].replaceDeepDown++; }
      }
    }

    // Write non-key fields first, then publish via release store on key.
    // XOR key with data hash to detect torn reads in probe().
    victimEntry->move  = static_cast<uint16_t>(move);
    victimEntry->depth = depth;
    victimEntry->value = value;
    victimEntry->type  = type;
    victimEntry->gen   = generation;
    victimEntry->eval  = eval;
    victimEntry->key.store(key ^ victimEntry->dataHash(), std::memory_order_release);
  }
}

std::optional<TT::Entry> TT::probe(const ZobristKey& key, const int threadIdx) {
#if TT_USE_MUTEX
  // used for debugging only
  std::lock_guard lock(ttMutex);
#endif

  TT_STAT_INC(statsSlots[threadIdx].numberOfProbes);
  if constexpr (TT_INSTRUMENTATION) {
    // ReSharper disable once CppDFAUnreachableCode
    instrSlots[threadIdx].probes++;
  }
  const TTCluster* const cluster = getCluster(key);

  // Scan all entries in the cluster for a key match.
  // XOR verification: storedKey ^ dataHash must equal the original key.
  // If a torn read corrupts any field (key or data), the XOR won't match,
  // causing a clean miss rather than returning corrupted data.
  for (auto& entry : cluster->entries) {
    const ZobristKey storedKey = entry.key.load(std::memory_order_acquire);

    // IMMEDIATELY copy the entry after key load to minimize torn-read window.
    // We copy before verification because dataHash() reads the data fields.
    Entry copy = entry; // Copy via copy constructor

    // Verify: storedKey XOR'd with data hash must equal the probe key.
    // This detects torn reads: if any field was corrupted, the reconstructed
    // key won't match, and we treat it as a miss (safe fallback).
    if ((storedKey ^ copy.dataHash()) == key) {
      // Restore the original key in the copy (it was stored XOR'd)
      copy.key.store(key, std::memory_order_relaxed);
      TT_STAT_INC(statsSlots[threadIdx].numberOfHits);

      // R6 instrumentation: track hit count and depth
      if constexpr (TT_INSTRUMENTATION) {
        // ReSharper disable once CppDFAUnreachableCode
        instrSlots[threadIdx].hits++;
        instrSlots[threadIdx].hitDepthSum += static_cast<uint64_t>(copy.depth);
        if (copy.depth == 0) { instrSlots[threadIdx].hitsDepth0++; }
      }

      return copy;
    }
  }
  TT_STAT_INC(statsSlots[threadIdx].numberOfMisses);
  return std::nullopt;
}

std::string TT::str() const {
  const std::size_t maxEntries = maxNumberOfClusters * CLUSTER_SIZE;
  const auto s = aggregateStats();
  return std::format(
    projectLocale,
    "TT: size {:L} MB max entries {:L} ({:L} clusters x {}) of size {:L} Bytes entries {:L} ({:L}%) puts {:L} "
    "updates {:L} collisions {:L} overwrites {:L} probes {:L} hits {:L} ({:L}%) misses {:L} ({:L}%)",
    sizeInByte / MB, maxEntries, maxNumberOfClusters, CLUSTER_SIZE, sizeof(Entry), s.numberOfEntries, hashFull() / 10,
    s.numberOfPuts, s.numberOfUpdates, s.numberOfCollisions, s.numberOfOverwrites, s.numberOfProbes,
    s.numberOfHits, s.numberOfProbes ? (s.numberOfHits * 100) / s.numberOfProbes : 0,
    s.numberOfMisses, s.numberOfProbes ? (s.numberOfMisses * 100) / s.numberOfProbes : 0);
}

// ReSharper disable once CppDFAConstantFunctionResult
std::string TT::instrumentationStr() const {
  if constexpr (!TT_INSTRUMENTATION) {
    return "TT Instrumentation (R6): disabled (set TT::TT_INSTRUMENTATION = true and rebuild)";
  }
  else {
    // ReSharper disable once CppDFAUnreachableCode
    const auto instr = aggregateInstrumentationStats();

    // Hit rate breakdown
    const double hitRate = instr.probes > 0 ? 100.0 * static_cast<double>(instr.hits) / static_cast<double>(instr.probes) : 0.0;
    const uint64_t hitsMainSearch = instr.hits - instr.hitsDepth0;

    // Hit depth averages (depth-0 entries contribute 0 to hitDepthSum, so hitDepthSum == main-search sum)
    const double avgHitDepthAll  = instr.hits > 0 ? static_cast<double>(instr.hitDepthSum) / static_cast<double>(instr.hits) : 0.0;
    const double avgHitDepthMain = hitsMainSearch > 0 ? static_cast<double>(instr.hitDepthSum) / static_cast<double>(hitsMainSearch) : 0.0;

    // Replacement breakdown
    const uint64_t totalReplacements = instr.replaceDepthDown + instr.replaceDepthUp + instr.replaceDepthEqual;
    const double downPct  = totalReplacements > 0 ? 100.0 * static_cast<double>(instr.replaceDepthDown) / static_cast<double>(totalReplacements) : 0.0;
    const double upPct    = totalReplacements > 0 ? 100.0 * static_cast<double>(instr.replaceDepthUp) / static_cast<double>(totalReplacements) : 0.0;
    const double equalPct = totalReplacements > 0 ? 100.0 * static_cast<double>(instr.replaceDepthEqual) / static_cast<double>(totalReplacements) : 0.0;
    const double avgDelta = totalReplacements > 0 ? static_cast<double>(instr.replaceDepthDeltaSum) / static_cast<double>(totalReplacements) : 0.0;
    const double avgVictimDepth = totalReplacements > 0 ? static_cast<double>(instr.replaceVictimDepthSum) / static_cast<double>(totalReplacements) : 0.0;

    // Deep entry eviction rate
    const double deepDownPct = instr.replaceDeepTotal > 0 ? 100.0 * static_cast<double>(instr.replaceDeepDown) / static_cast<double>(instr.replaceDeepTotal) : 0.0;

    return std::format(
      projectLocale,
      "TT Instrumentation (R6):\n"
      "  Probes: {:L}, Hits: {:L} ({:.1f}%), Misses: {:L}\n"
      "    QSearch hits (depth=0): {:>12L} ({:.1f}% of hits)\n"
      "    Main search hits:       {:>12L} ({:.1f}% of hits)\n"
      "  Avg hit depth (all):         {:.1f}\n"
      "  Avg hit depth (main search): {:.1f}\n"
      "  Replacements: {:L} total, avg victim depth: {:.1f}\n"
      "    Depth down  (harmful):    {:>12L} ({:5.1f}%)\n"
      "    Depth up    (beneficial): {:>12L} ({:5.1f}%)\n"
      "    Depth equal:              {:>12L} ({:5.1f}%)\n"
      "  Avg replacement depth delta: {:+.2f} (positive = upgrades dominate)\n"
      "  Deep entries (depth>={:d}): {:L} replaced, {:L} by shallower ({:.1f}%)",
      instr.probes, instr.hits, hitRate, instr.probes - instr.hits,
      instr.hitsDepth0, instr.hits > 0 ? 100.0 * static_cast<double>(instr.hitsDepth0) / static_cast<double>(instr.hits) : 0.0,
      hitsMainSearch, instr.hits > 0 ? 100.0 * static_cast<double>(hitsMainSearch) / static_cast<double>(instr.hits) : 0.0,
      avgHitDepthAll,
      avgHitDepthMain,
      totalReplacements, avgVictimDepth,
      instr.replaceDepthDown, downPct,
      instr.replaceDepthUp, upPct,
      instr.replaceDepthEqual, equalPct,
      avgDelta,
      DEEP_ENTRY_THRESHOLD, instr.replaceDeepTotal, instr.replaceDeepDown, deepDownPct);
  }
}
