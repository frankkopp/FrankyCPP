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
//   - Thread-safe key field (std::atomic<ZobristKey>) for Lazy SMP
//     On x86 acquire/release compiles to plain mov - zero overhead vs non-atomic
//     Verified by: static_assert(is_always_lock_free) + static_assert(sizeof == 8)
//
  // Entry Structure (32 bytes):
  //   - key:          64-bit pawn Zobrist key (std::atomic for SMP safety)
  //   - midvalue:     16-bit midgame pawn structure score
  //   - endvalue:     16-bit endgame pawn structure score
  //   - passedWhite:  64-bit bitboard of white passed pawns
  //   - passedBlack:  64-bit bitboard of black passed pawns
//
// Thread Safety (Lazy SMP):
//   probe(): Copy-on-read pattern - returns a COPY of the entry, not a pointer.
//            This eliminates races where another thread overwrites the entry
//            between key verification and value reads. The copy is made
//            immediately after the key load (acquire), then verified.
//   put()  : Writes value fields first, then key with memory_order_release.
//            Any thread that loads key with acquire will see all prior writes.
//   getEntryPtr(): For put() use only - provides slot pointer to avoid rehash.
//
// Prefetching:
//   EVAL_PREFETCH macro prefetches entry into CPU cache before evaluation.
//   Call as early as possible before the actual lookup.
//
// Usage:
//   PawnTT pawnCache(4);  // 4 MB cache
//   auto entry = pawnCache.probe(pawnKey);
//   if (entry) {
//     // Cache hit - use entry->midvalue and entry->endvalue (from copy)
//   } else {
//     // Cache miss - calculate pawn eval, then store
//     pawnCache.put(pawnCache.getEntryPtr(pawnKey), pawnKey, score);
//   }
//
//=============================================================================

#include "types/types.h"
#include <atomic>
#include <cstring>
#include <format>
#include <optional>
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
#define EVAL_PREFETCH thread().evaluator.prefetch(p.getPawnZobristKey())
#else
#define EVAL_PREFETCH void(0);
#endif

/// Pawn structure evaluation cache using heap memory with simple hash indexing.
/// Size is always a power of two.
/// Thread-safe for Lazy SMP via atomic key field (acquire/release, zero overhead on x86).
namespace engine {
  using namespace chess;

  class PawnTT {

  public:
    static constexpr int CacheLineSize        = 64;
    static constexpr uint64_t DEFAULT_TT_SIZE = 2; // MByte
    static constexpr uint64_t MAX_SIZE_MB     = 4'096;

    /// Entry struct storing cached pawn evaluation scores and passed-pawn bitboards.
    struct Entry {
      std::atomic<ZobristKey> key{0}; ///< Pawn-specific Zobrist key (atomic for SMP safety)
      Value midvalue{VALUE_NONE};     ///< Midgame pawn structure score
      Value endvalue{VALUE_NONE};     ///< Endgame pawn structure score
      Bitboard passedWhite{BbZero};   ///< Bitboard of white passed pawns
      Bitboard passedBlack{BbZero};   ///< Bitboard of black passed pawns

      /// Default constructor
      Entry() = default;

      /// Copy constructor - needed because atomic has deleted copy constructor.
      /// Uses memcpy for all payload fields to ensure exact byte representation.
      Entry(const Entry& other);

      /// Copy assignment - needed because atomic has deleted copy assignment.
      Entry& operator=(const Entry& other);

      /// Returns the key with acquire semantics for thread-safe read.
      [[nodiscard]] ZobristKey getKey() const {
        return key.load(std::memory_order_acquire);
      }

      /// Returns string representation for debugging.
      [[nodiscard]] std::string str() const {
        return std::format("id {} midvalue {} endvalue {} passedW {:016x} passedB {:016x}",
                           key.load(std::memory_order_relaxed), midvalue, endvalue,
                           passedWhite.value(), passedBlack.value());
      }

      std::ostream& operator<<(std::ostream& os) const {
        os << this->str();
        return os;
      }
    };

    /// Size of all non-atomic payload fields in Entry (midvalue through passedBlack).
    /// Defined after Entry is complete so sizeof(Entry) is available.
    static constexpr auto ENTRY_PAYLOAD_SIZE = sizeof(Entry) - sizeof(std::atomic<ZobristKey>);

    // Compile-time guarantees that the atomic key has zero size/performance overhead.
    // If either assert fires, switch to Option B (XOR key trick) - see PLAN_Lazy_SMP_MultiThreading.md
    static_assert(std::atomic<ZobristKey>::is_always_lock_free,
                  "PawnTT: atomic key must be lock-free (no hidden mutex). Switch to XOR trick if this fires.");
    static_assert(sizeof(std::atomic<ZobristKey>) == sizeof(ZobristKey),
                  "PawnTT: atomic key must not inflate Entry size. Switch to XOR trick if this fires.");

    // struct Entry has 32 Bytes
    static constexpr uint64_t ENTRY_SIZE = sizeof(Entry);
    static_assert(CacheLineSize % ENTRY_SIZE == 0, "Cluster size incorrect");

  private:
    // threads for clearing hash
    unsigned int noOfThreads = 1;

    // Number of active SMP search threads. 1 = single-thread mode (default).
    // When > 1: statistics may be approximate (no locking for diagnostic counters).
    // Set by Search before each search via setSmpThreads().
    int numSmpThreads = 1;

    // size and fill info
    uint64_t sizeInByte            = 0;
    std::size_t maxNumberOfEntries = 0;
    std::size_t hashKeyMask        = 0;
    std::size_t numberOfEntries    = 0;

    mutable uint64_t numberOfQueries = 0;
    mutable uint64_t numberOfHits    = 0; // entries with identical key found
    mutable uint64_t numberOfMisses  = 0; // no entry with key found

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

    /// Sets the number of SMP threads for this search.
    /// When > 1, statistics counters may be approximate (acceptable for diagnostics).
    /// Call before each search begins.
    /// @param threads  Number of active search threads (1 = single-threaded)
    void setSmpThreads(const int threads) { numSmpThreads = threads; }

    /// Returns the current SMP thread count.
    [[nodiscard]] int getSmpThreads() const { return numSmpThreads; }

    /// Stores a pawn evaluation score and passed-pawn bitboards in the cache.
    /// As usually a query happens before storing, the entry pointer is typically
    /// already known from getEntryPtr(). This avoids a redundant hash lookup.
    /// @param entryPtr     Pointer to the entry slot (from getEntryPtr)
    /// @param key          Pawn Zobrist key
    /// @param score        Pawn structure score (midgame + endgame)
    /// @param passedWhite  Bitboard of white passed pawns
    /// @param passedBlack  Bitboard of black passed pawns
    void put(Entry* entryPtr, ZobristKey key, Score score, Bitboard passedWhite, Bitboard passedBlack);

    /// Probes the PawnTT for an entry matching the key.
    /// Returns a COPY of the entry for thread safety (copy-on-read pattern).
    /// This prevents races where another thread overwrites the entry between
    /// key check and value read.
    /// Updates hit/miss statistics.
    /// @param key  Pawn Zobrist key (must be non-zero; key==0 always returns nullopt)
    /// @return     Copy of matching entry, or nullopt if not found or key==0
    [[nodiscard]] std::optional<Entry> probe(ZobristKey key) const;

    /// Returns a pointer to the entry for the given pawn key.
    /// WARNING: For put() only! Do NOT read values through this pointer.
    /// Use probe() to safely read entry values (copy-on-read pattern).
    /// @param key  Pawn Zobrist key
    /// @return     Pointer to the entry slot (for put() use only)
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

} // namespace engine

#endif // FRANKYCPP_PAWNTT_H
