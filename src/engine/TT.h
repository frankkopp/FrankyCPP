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

#ifndef FRANKYCPP_TT_H
#define FRANKYCPP_TT_H

//=============================================================================
// TT.h - Transposition Table
//=============================================================================
//
// The Transposition Table (TT) caches search results to avoid re-searching
// positions that have been visited before via different move orders.
// Depends on: types.h
//
// Design:
//   - Heap-allocated array of Entry structs
//   - Size is always a power of two for efficient hash masking
//   - Single entry per hash slot (no buckets - testing showed 20% slower)
//   - Struct with bitfields is 9% faster than manual bit manipulation
//   - Not thread-safe (no synchronization)
//
// Entry Structure (16 bytes):
//   - key:       64-bit Zobrist key for collision detection
//   - move:      16-bit best move (without sort value)
//   - eval:      16-bit static evaluation
//   - value:     16-bit search value
//   - depth:     7-bit search depth (0-127)
//   - age:       3-bit generation counter (0-7)
//   - type:      2-bit value type (NONE, EXACT, ALPHA, BETA)
//   - mateThreat: 1-bit flag
//
// Prefetching:
//   TT_PREFETCH macro prefetches entry into CPU cache before probe.
//   Significantly improves lookup performance.
//
// Usage:
//   TT tt(64);  // 64 MB table
//   tt.put(key, depth, move, value, EXACT, eval);
//   const TT::Entry* entry = tt.probe(key);
//   if (entry && entry->depth >= depth) { ... }
//
//=============================================================================

#include <iosfwd>

#include "common/gtest_friends.h"
#include "types/types.h"

// pre-fetching of TT entries into CPU caches
#ifdef __GNUC__
#include <emmintrin.h>
#define TT_ENABLE_PREFETCH
#elif _MSC_VER
#include <intrin.h>
#define TT_ENABLE_PREFETCH
#endif

#ifdef TT_ENABLE_PREFETCH
#define TT_PREFETCH tt->prefetch(p.getZobristKey())
#else
#define TT_PREFETCH void(0);
#endif

/**
 * TT implementation using heap memory and simple hash for entries.
 * The number of entries is always a power of two fitting into the given size.
 * It is not yet thread-safe as it has no synchronization.
 *
 * Tests have shown that an implementation with a struct and bitfields is
 * more efficient than using only one 64-bit data field with manual bit shifting
 * and masking (~9% slower).
 * Also, using buckets has not shown significant strength improvements and is
 * much slower (~20% slower).
 */
class TT {
public:
  static constexpr int CacheLineSize        = 64;
  static constexpr uint64_t DEFAULT_TT_SIZE = 2;// MByte
  static constexpr uint64_t MAX_SIZE_MB     = 32'768;

  // TT Entry
  //  Key key       = 0;         // 64 bit
  //  uint16_t move = MOVE_NONE; // 16 bit (last 16-bit omitting value part - cast to Move)
  //  Value eval    = VALUE_NONE;// 16 bit signed
  //  Value value   = VALUE_NONE;// 16 bit signed
  //  Depth depth : 7;           // 0-127
  //  uint8_t age : 3;           // 0-7
  //  ValueType type : 2;        // 4 values
  //  bool mateThreat : 1;       // 1-bit bool
  struct Entry {
    // sorted by size to achieve the smallest struct size
    // using bitfield for the smallest size
    ZobristKey key = 0;         // 64 bit
    uint16_t move  = 0;         // MOVE_NONE as 16-bit
    Value eval     = VALUE_NONE;// 16-bit signed
    Value value    = VALUE_NONE;// 16-bit signed
    uint8_t depth : 7 {};        // 0-127
    uint8_t age : 3 {};         // 0-7
    ValueType type : 2 {};      // 4 values
    bool mateThreat : 1 {};     // 1-bit bool
    friend std::ostream& operator<<(std::ostream& os, const Entry& entry);
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

  // statistics
  mutable uint64_t numberOfPuts       = 0;
  mutable uint64_t numberOfCollisions = 0;
  mutable uint64_t numberOfOverwrites = 0;
  mutable uint64_t numberOfUpdates    = 0;
  mutable uint64_t numberOfProbes     = 0;
  mutable uint64_t numberOfHits       = 0;// entries with identical key found
  mutable uint64_t numberOfMisses     = 0;// no entry with key found

  // this array holds the actual entries for the transposition table
  std::unique_ptr<Entry[]> _data = std::make_unique<Entry[]>(maxNumberOfEntries);

public:
  /// Creates a TT with default size (2 MB).
  TT() : TT(DEFAULT_TT_SIZE) {}

  /// Creates a TT with the specified size.
  /// Size will be reduced to the next lowest power of 2.
  /// @param newSizeInMByte  Size in megabytes (limited to 32,768 MB)
  explicit TT(uint64_t newSizeInMByte);

  ~TT() = default;

  // disallow copies and moves
  TT(TT const& tt)          = delete;
  TT& operator=(const TT&)  = delete;
  TT(TT const&& tt)         = delete;
  TT& operator=(const TT&&) = delete;

  /// Changes the size of the transposition table and clears all entries.
  /// If set to 0 MB, TT will ensure at least 1 entry, which can be used as an uninitialized sentinel.
  /// @param newSizeInMByte  Size in megabytes, reduced to next lowest power of 2.
  ///                        Limited to 32,768 MB.
  void resize(uint64_t newSizeInMByte);

  /// Clears the transposition table by resetting all entries to zero.
  void clear();

  /// Stores a position in the transposition table.
  /// The move will be stripped of any sort value before storing, as value
  /// is stored separately. This avoids surprising behavior where MOVE_NONE
  /// might appear to have a value.
  /// @param key    Position key (usually Zobrist key)
  /// @param depth  Search depth (0 to DEPTH_MAX, usually 127)
  /// @param move   Best move of the node (for BETA: best move until cutoff)
  /// @param value  Search value between VALUE_MIN and VALUE_MAX
  /// @param type   Value bound type: EXACT, ALPHA, or BETA
  /// @param eval   Static evaluation of the position
  void put(ZobristKey key, Depth depth, Move move, Value value, ValueType type, Value eval);

  /// Retrieves an entry matching the given key without updating statistics.
  /// @param key  Position key (usually Zobrist key)
  /// @return     Pointer to matching entry, or nullptr if not found
  const Entry* getMatch(const ZobristKey key) const {
    const Entry* const entryPtr = getEntryPtrConst(key);
    return entryPtr->key == key ? entryPtr : nullptr;
  }

  /// Probes the TT for an entry matching the key.
  /// Updates hit/miss statistics and decreases age of found entry.
  /// @param key  Position key (usually Zobrist key)
  /// @return     Pointer to matching entry, or nullptr if not found
  const Entry* probe(const ZobristKey& key);

  /// Ages all entries by incrementing their age counter.
  /// Called at the start of each new search to help with replacement.
  void ageEntries();

  /// Returns how full the transposition table is in permill (0-1000).
  /// Used for UCI "hashfull" info output.
  /// @return  Fill level in permill
  [[nodiscard]] int hashFull() const {
    return static_cast<int>((1000 * numberOfEntries) / maxNumberOfEntries);
  };

  /// Prefetches the TT entry for the given key into the CPU cache.
  ///
  /// Call this as early as possible before probe(), ideally with other work
  /// in between (e.g., move generation, evaluation setup) to give the memory
  /// subsystem time to fetch the data. The prefetch is asynchronous and does
  /// not block execution. Optimal timing is 100-300 cycles before the actual
  /// memory access, depending on memory latency.
  ///
  /// Uses _MM_HINT_T0 which fetches into all cache levels (L1, L2, L3).
  /// Significantly improves probe() performance by hiding memory latency,
  /// especially for large TT sizes that don't fit in CPU cache.
  ///
  /// @param key  Position key to prefetch
  void prefetch(const ZobristKey key) const {
#ifdef TT_ENABLE_PREFETCH
    _mm_prefetch((reinterpret_cast<const char*>(&_data[(key & hashKeyMask)])), _MM_HINT_T0);
#else
    (void)key;
#endif
  }

  /// Returns a string representation of the TT instance for debugging.
  /// @return  Debug string with size and statistics
  std::string str() const;

private:
  /// Generates the index from the position key using bitmask.
  /// @param key  Position key
  /// @return     Array index for the entry
  std::size_t getHash(const ZobristKey key) const {
    return key & hashKeyMask;
  }

  /// Returns a mutable pointer to the entry for the given key.
  /// @param key  Position key
  /// @return     Pointer to the entry slot
  Entry* getEntryPtr(const ZobristKey key) const {
    return &_data[getHash(key)];
  }

  /// Returns a const pointer to the entry for the given key.
  /// @param key  Position key
  /// @return     Const pointer to the entry slot
  const Entry* getEntryPtrConst(const ZobristKey key) const {
    return &_data[getHash(key)];
  }

public:
  // === Getters ===

  /// Returns the size of the TT in bytes.
  uint64_t getSizeInByte() const { return sizeInByte; }

  /// Returns the maximum number of entries the TT can hold.
  std::size_t getMaxNumberOfEntries() const { return maxNumberOfEntries; }

  /// Returns the current number of entries stored.
  std::size_t getNumberOfEntries() const { return numberOfEntries; }

  /// Returns the total number of put() calls.
  uint64_t getNumberOfPuts() const { return numberOfPuts; }

  /// Returns the number of hash collisions (different position, same slot).
  uint64_t getNumberOfCollisions() const { return numberOfCollisions; }

  /// Returns the number of overwrites (replaced existing entry).
  uint64_t getNumberOfOverwrites() const { return numberOfOverwrites; }

  /// Returns the number of updates (same position, updated entry).
  uint64_t getNumberOfUpdates() const { return numberOfUpdates; }

  /// Returns the total number of probe() calls.
  uint64_t getNumberOfProbes() const { return numberOfProbes; }

  /// Returns the number of successful probes (entry with matching key found).
  uint64_t getNumberOfHits() const { return numberOfHits; }

  /// Returns the number of failed probes (no matching entry found).
  uint64_t getNumberOfMisses() const { return numberOfMisses; }

  /// Returns the number of threads used for clearing.
  unsigned int getThreads() const { return noOfThreads; }

  /// Sets the number of threads used for clearing.
  /// @param threads  Number of threads (minimum 1)
  void setThreads(const int threads) { noOfThreads = threads > 0 ? static_cast<unsigned int>(threads) : 1u; }

  /// Converts a ValueType to its string representation.
  /// @param type  ValueType to convert
  /// @return      "NONE", "EXACT", "ALPHA", or "BETA"
  static std::string str(const ValueType type) {
    switch (type) {
      case NONE:
        return "NONE";
      case EXACT:
        return "EXACT";
      case ALPHA:
        return "ALPHA";
      case BETA:
        return "BETA";
    }
    return "";
  }

  FRIEND_TEST(TT_Test, put);
  FRIEND_TEST(TT_Test, get);
  FRIEND_TEST(TT_Test, probe);
};

#endif// FRANKYCPP_TT_H
