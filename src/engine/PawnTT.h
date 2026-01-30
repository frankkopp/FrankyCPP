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

#ifndef FRANKYCPP_PAWNTT_H
#define FRANKYCPP_PAWNTT_H

//=============================================================================
// PawnTT.h - Pawn Structure Evaluation Cache
//=============================================================================
//
// PawnTT caches pawn structure evaluations to avoid recalculating them.
// Pawn structure changes infrequently (only on pawn moves or captures),
// so caching provides significant speedup.
// Depends on: types.h
//
// Design:
//   - Heap-allocated array of Entry structs
//   - Size is always a power of two for efficient hash masking
//   - Uses pawn-specific Zobrist key (only pawn positions contribute)
//   - Single entry per hash slot (no buckets)
//   - Not thread-safe (no synchronization)
//
// Entry Structure (16 bytes):
//   - key:      64-bit pawn Zobrist key
//   - midvalue: 16-bit midgame pawn structure score
//   - endvalue: 16-bit endgame pawn structure score
//
// Prefetching:
//   EVAL_PREFETCH macro prefetches entry into CPU cache before evaluation.
//   Call as early as possible before the actual lookup.
//
// Usage:
//   PawnTT pawnCache(4);  // 4 MB cache
//   Entry* entry = pawnCache.getEntryPtr(pawnKey);
//   if (entry->key == pawnKey) {
//     // Cache hit - use entry->midvalue and entry->endvalue
//   } else {
//     // Cache miss - calculate pawn eval, then store
//     pawnCache.put(entry, pawnKey, score);
//   }
//
//=============================================================================

#include "types/types.h"
#include <format>
#include <string>

// pre-fetching of TT entries into CPU caches
#ifdef __GNUC__
#include <emmintrin.h>
#define EVAL_ENABLE_PREFETCH
#elif _MSC_VER
#include <intrin.h>
#define EVAL_ENABLE_PREFETCH
#endif

#ifdef EVAL_ENABLE_PREFETCH
#define EVAL_PREFETCH evaluator->prefetch(p.getPawnZobristKey())
#else
#define EVAL_PREFETCH void(0);
#endif

/// Pawn structure evaluation cache using heap memory with simple hash indexing.
/// Size is always a power of two. Not thread-safe.
class PawnTT {

public:
  static constexpr int CacheLineSize        = 64;
  static constexpr uint64_t DEFAULT_TT_SIZE = 2;// MByte
  static constexpr uint64_t MAX_SIZE_MB     = 4'096;

  /// Entry struct storing cached pawn evaluation scores.
  struct Entry {
    ZobristKey key = 0;      ///< Pawn-specific Zobrist key
    Value midvalue = VALUE_NONE;  ///< Midgame pawn structure score
    Value endvalue = VALUE_NONE;  ///< Endgame pawn structure score

    /// Returns string representation for debugging.
    [[nodiscard]] std::string str() const {
      return std::format("id {} midvalue {} endvalue {}", key, midvalue, endvalue);
    }

    std::ostream& operator<<(std::ostream& os) const {
      os << this->str();
      return os;
    }
  };

  // struct Entry has 16 Byte
  static constexpr uint64_t ENTRY_SIZE = sizeof(Entry);
  static_assert(CacheLineSize % ENTRY_SIZE == 0, "Cluster size incorrect");

private:
  // threads for clearing hash
  unsigned int noOfThreads = 1;

  // size and fill info
  uint64_t sizeInByte            = 0;
  std::size_t maxNumberOfEntries = 0;
  std::size_t hashKeyMask        = 0;
  std::size_t numberOfEntries    = 0;

  mutable uint64_t numberOfQueries = 0;
  mutable uint64_t numberOfHits    = 0;// entries with identical key found
  mutable uint64_t numberOfMisses  = 0;// no entry with key found

  mutable uint64_t numberOfPuts       = 0;
  mutable uint64_t numberOfCollisions = 0;
  mutable uint64_t numberOfOverwrites = 0;
  mutable uint64_t numberOfUpdates    = 0;

  // this array hold the actual entries for the transposition table
  std::unique_ptr<Entry[]> _data = std::make_unique<Entry[]>(maxNumberOfEntries);

public:
  /// Creates a PawnTT with default size (2 MB).
  PawnTT() : PawnTT(DEFAULT_TT_SIZE) {}

  /// Creates a PawnTT with the specified size.
  /// Size will be reduced to the next lowest power of 2.
  /// @param newSizeInMByte  Size in megabytes (limited to 4,096 MB)
  explicit PawnTT(uint64_t newSizeInMByte);

  ~PawnTT() = default;

  // disallow copies and moves
  PawnTT(PawnTT const& tt)          = delete;
  PawnTT& operator=(const PawnTT&)  = delete;
  PawnTT(PawnTT const&& tt)         = delete;
  PawnTT& operator=(const PawnTT&&) = delete;

  /// Changes the size of the pawn cache and clears all entries.
  /// @param newSizeInMByte  Size in megabytes, reduced to next lowest power of 2.
  ///                        Limited to 4,096 MB.
  void resize(uint64_t newSizeInMByte);

  /// Clears the pawn cache by resetting all entries to zero.
  void clear();

  /// Stores a pawn evaluation score in the cache.
  /// As usually a query happens before storing, the entry pointer is typically
  /// already known from getEntryPtr(). This avoids a redundant hash lookup.
  /// @param entryPtr  Pointer to the entry slot (from getEntryPtr)
  /// @param key       Pawn Zobrist key
  /// @param score     Pawn structure score (midgame + endgame)
  void put(Entry* entryPtr, ZobristKey key, Score score);

  /// Returns a pointer to the entry for the given pawn key.
  /// The entry may or may not match the key - caller must verify.
  /// @param key  Pawn Zobrist key
  /// @return     Pointer to the entry slot
  Entry* getEntryPtr(const ZobristKey key) const {
    return &_data[getHash(key)];
  }

  /// Generates the index from the pawn key using bitmask.
  /// @param key  Pawn Zobrist key
  /// @return     Array index for the entry
  std::size_t getHash(const ZobristKey key) const {
    return key & hashKeyMask;
  }

  /// Returns a string representation for debugging.
  /// @return  Debug string with size and statistics
  std::string str();

  /// Prefetches the cache entry for the given key into CPU cache.
  /// Call as early as possible before getEntryPtr(), ideally with other
  /// work in between to give the memory subsystem time to fetch the data.
  /// @param key  Pawn Zobrist key to prefetch
#ifdef EVAL_ENABLE_PREFETCH
  void prefetch(const ZobristKey key) {
#ifdef __GNUC__
    _mm_prefetch((reinterpret_cast<const char*>(&_data[(key & hashKeyMask)])), _MM_HINT_T0);
#elif _MSC_VER
    _mm_prefetch((reinterpret_cast<const char*>(&_data[(key & hashKeyMask)])), _MM_HINT_T0);
#endif
  }
#endif

public:
  // === Getters ===

  /// Returns the size of the cache in bytes.
  uint64_t getSizeInByte() const { return sizeInByte; }

  /// Returns the maximum number of entries the cache can hold.
  std::size_t getMaxNumberOfEntries() const { return maxNumberOfEntries; }

  /// Returns the current number of entries stored.
  std::size_t getNumberOfEntries() const { return numberOfEntries; }

  /// Returns the number of cache hits.
  uint64_t getNumberOfHits() const { return numberOfHits; }

  /// Returns the number of cache misses.
  uint64_t getNumberOfMisses() const { return numberOfMisses; }

  /// Returns the number of entry updates (same key, new value).
  uint64_t getNumberOfUpdates() const { return numberOfUpdates; }

  /// Returns the total number of put() calls.
  uint64_t getNumberOfPuts() const { return numberOfPuts; }

  /// Returns the number of hash collisions (different key, same slot).
  uint64_t getNumberOfCollisions() const { return numberOfCollisions; }
};

#endif// FRANKYCPP_PAWNTT_H
