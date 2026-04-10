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
//   - Heap-allocated array of TTCluster structs (4 entries per cluster)
//   - Each cluster is alignas(64) = exactly one cache line (4 × 16B = 64B)
//   - Number of clusters is always a power of two for efficient hash masking
//   - Bucket design eliminates false sharing across CPU cores under SMP
//   - 4-way associative: probe scans 4 entries per cluster for matches
//   - Replacement policy: depth-preferred with age tiebreak
//   - Struct with bitfields is 9% faster than manual bit manipulation
//   - Thread-safe key field (std::atomic<ZobristKey>) for Lazy SMP
//     On x86 acquire/release compiles to plain mov - zero overhead vs non-atomic
//     Verified by: static_assert(is_always_lock_free) + static_assert(sizeof == 8)
//
// Entry Structure (16 bytes):
//   - key:        64-bit Zobrist key (std::atomic) for collision detection
//   - move:       16-bit best move (without sort value)
//   - eval:       16-bit static evaluation
//   - value:      16-bit search value
//   - depth:      7-bit search depth (0-127)
//   - age:        3-bit generation counter (0-7)
//   - type:       2-bit value type (NONE, EXACT, ALPHA, BETA)
//   - mateThreat: 1-bit flag
//
// Cluster Structure (64 bytes = 1 cache line):
//   - entries[4]: 4 × Entry = 4 × 16B = 64B
//   - alignas(64) ensures each cluster starts at a cache line boundary
//   - Single _mm_prefetch pulls the entire cluster into L1/L2/L3
//
// Thread Safety (Lazy SMP):
//   XOR Key Verification:
//     - Key is stored as: storedKey = originalKey ^ dataHash
//     - Probe verifies: (storedKey ^ dataHash) == probeKey
//     - If a torn read corrupts ANY field (key or data), the XOR won't match
//     - Corrupted entries are treated as misses - no bad data is ever returned
//     - This eliminates the race window between key check and data copy
//   TODO: Consider removing this in the future as it might actually harm strength
//         as tests from Stockfish have shown.
//
//   put()  : writes non-key fields first, then key XOR'd with dataHash (release)
//   probe(): loads key (acquire), copies entry, verifies XOR, returns if valid
//            The copy may still contain torn data, but XOR verification ensures
//            it's "self-consistent" - all fields from the same write operation.
//   age-- in probe(): safe in single-thread mode (default). Skipped under SMP
//   (numSmpThreads > 1) because it is a read-modify-write on a packed bitfield
//   byte shared with depth/type - a data race. Behavioral impact is minimal:
//   same-depth replacement tiebreak slightly more aggressive, deep entries
//   marginally more vulnerable to eviction. No impact on single-thread behavior.
//
// Usage:
//   TT tt(64);  // 64 MB table
//   tt.put(key, depth, move, value, EXACT, eval);
//   if (auto entry = tt.probe(key); entry && entry->depth >= depth) { ... }
//
//=============================================================================

#include <array>
#include <atomic>
#include <cstring>
#include <iosfwd>
#include <mutex>
#include <optional>

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
 * TT implementation using heap memory and clustered hash buckets.
 * Each cluster holds 4 entries aligned to a 64-byte cache line boundary.
 * The number of clusters is always a power of two fitting into the given size.
 * Thread-safe for Lazy SMP via atomic key field (acquire/release, zero overhead on x86).
 *
 * Tests have shown that an implementation with a struct and bitfields is
 * more efficient than using only one 64-bit data field with manual bit shifting
 * and masking (~9% slower).
 *
 * The bucket design provides:
 * - Cache-line alignment eliminates false sharing under SMP
 * - 4-way associativity reduces collision evictions
 * - Depth-preferred + age tiebreak replacement policy
 * - Single prefetch loads the entire bucket (4 entries)
 */
// Forward-declare test classes at global scope so FRIEND_TEST inside namespace engine works
FRIEND_TEST_FWD_DECL(TT_Test, put);
FRIEND_TEST_FWD_DECL(TT_Test, get);
FRIEND_TEST_FWD_DECL(TT_Test, probe);

namespace engine {
  using namespace chess;

  class TT {
  public:
    static constexpr int CacheLineSize        = 64;
    static constexpr int CLUSTER_SIZE         = 4; // entries per cluster
    static constexpr uint64_t DEFAULT_TT_SIZE = 2; // MByte
    static constexpr uint64_t MAX_SIZE_MB     = 32'768;

    // TT Entry
    //  atomic<Key> key = 0;  // 64 bit  (std::atomic, acquire/release, plain mov on x86)
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
      //
      // key is atomic for Lazy SMP thread safety.
      // On x86, acquire/release memory order = plain mov (no fence, no lock prefix).
      // Compile-time verified: is_always_lock_free + sizeof == sizeof(ZobristKey).
      //
      // XOR Key Verification Protocol:
      //   Store: key is stored as (originalKey ^ dataHash) after writing data fields
      //   Load:  verify (storedKey ^ dataHash) == probeKey to detect torn reads
      // If any field is corrupted by a torn read, the XOR won't match = clean miss.
      std::atomic<ZobristKey> key{0}; // 64 bit - atomic for SMP safety
      uint16_t move = 0;              // MOVE_NONE as 16-bit
      Value eval    = VALUE_NONE;     // 16-bit signed
      Value value   = VALUE_NONE;     // 16-bit signed
      uint8_t depth : 7 {};           // 0-127
      uint8_t age : 3 {};             // 0-7
      ValueType type : 2 {};          // 4 values
      bool mateThreat : 1 {};         // 1-bit bool

      // Default constructor
      Entry() = default;

      // TT_USE_XOR_KEY: Toggle XOR key verification for torn-read detection.
      // Set to 1 (default): Full XOR verification - detects torn reads, slight overhead
      // Set to 0: Disabled - for performance comparison (dataHash returns 0, XOR is no-op)
      // The compiler will optimize away the XOR when dataHash() returns constant 0.
#define TT_USE_XOR_KEY 1

      // Computes a hash of all non-key data fields for XOR key verification.
      // The 8 bytes after key (move, eval, value, bitfields) are contiguous.
      // Used to detect torn reads: storedKey ^ dataHash == originalKey.
      // If a torn read corrupts any field, the reconstructed key won't match.
      [[nodiscard]] uint64_t dataHash() const {
#if TT_USE_XOR_KEY
        uint64_t data = 0;
        std::memcpy(&data, &move, sizeof(data));
        return data;
#else
        return 0; // Compiler eliminates XOR: key ^ 0 == key
#endif
      }

      // Copy constructor - needed because atomic has deleted copy constructor.
      // Uses memcpy for non-key fields to preserve exact byte representation
      // (including any padding/unused bits) for dataHash() XOR verification.
      Entry(const Entry& other)
          : key(other.key.load(std::memory_order_relaxed)) {
        std::memcpy(&move, &other.move, sizeof(uint64_t));
      }

      // Copy assignment - needed because atomic has deleted copy assignment.
      // Uses memcpy for non-key fields to preserve exact byte representation.
      Entry& operator=(const Entry& other) {
        if (this != &other) {
          key.store(other.key.load(std::memory_order_relaxed), std::memory_order_relaxed);
          std::memcpy(&move, &other.move, sizeof(uint64_t));
        }
        return *this;
      }

      friend std::ostream& operator<<(std::ostream& os, const Entry& entry);
    };

    // Compile-time guarantees that the atomic key has zero size/performance overhead.
    // If either assert fires, switch to Option B (XOR key trick) - see PLAN_Lazy_SMP_MultiThreading.md
    static_assert(std::atomic<ZobristKey>::is_always_lock_free,
                  "TT: atomic key must be lock-free (no hidden mutex). Switch to XOR trick if this fires.");
    static_assert(sizeof(std::atomic<ZobristKey>) == sizeof(ZobristKey),
                  "TT: atomic key must not inflate Entry size. Switch to XOR trick if this fires.");

    // struct Entry has 16 Byte
    static constexpr uint64_t ENTRY_SIZE = sizeof(Entry);
    static_assert(sizeof(Entry) == 16, "TT Entry must remain 16 bytes for cache alignment");

    // TTCluster: 4 entries aligned to one cache line (64 bytes).
    // alignas(64) guarantees that std::make_unique<TTCluster[]> uses aligned operator new
    // (C++17 mandates aligned allocation when alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__).
    struct alignas(CacheLineSize) TTCluster {
      std::array<Entry, CLUSTER_SIZE> entries;
    };

    static_assert(sizeof(TTCluster) == CacheLineSize, "TTCluster must be exactly one cache line (64 bytes)");
    static_assert(alignof(TTCluster) == CacheLineSize,
                  "TTCluster must be cache-line aligned. Guarantees aligned heap allocation via C++17.");
    static constexpr uint64_t CLUSTER_BYTE_SIZE = sizeof(TTCluster);

  private:
    // threads for clearing hash
    unsigned int noOfThreads = 1;

    // Number of active SMP search threads. 1 = single-thread mode (default).
    // When > 1: probe() skips age-- to avoid a data race on the packed bitfield byte.
    // Set by Search before each search via setSmpThreads().
    int numSmpThreads = 1;

    // TT_USE_MUTEX: Optional mutex for debugging TT race conditions.
    // Set to 1 to enable synchronized access (significantly slower).
    // Set to 0 for normal lockless operation with copy-on-read (like Stockfish).
    // The copy-on-read approach in probe() ensures callers receive a consistent
    // snapshot even without synchronization - torn reads may produce stale/mixed
    // data but won't cause crashes since the copy is final once returned.
#define TT_USE_MUTEX 0
#if TT_USE_MUTEX
    mutable std::mutex ttMutex{};
#endif

    // size and fill info — read-only after resize(), shares cache line with _data pointer.
    uint64_t sizeInByte             = 0;
    std::size_t maxNumberOfClusters = 0;
    std::size_t clusterMask         = 0;

    // this array holds the actual clusters for the transposition table
    std::unique_ptr<TTCluster[]> _data = std::make_unique<TTCluster[]>(maxNumberOfClusters); // NOLINT(*-avoid-c-arrays)

    // Statistics counters — per-thread slots, each on its own cache line.
    // probe()/put() index into statsSlots[threadIdx] so each thread writes
    // exclusively to its own cache line. This eliminates both false sharing
    // (fixed by prior alignas(64) padding) and true sharing (all threads
    // contending on a single Stats cache line).
    // Aggregation methods sum across active slots on demand (cold path only).
    // Measured impact: 5.7x → ~13x scaling at 16 threads.

    struct alignas(CacheLineSize) Stats {
      std::size_t numberOfEntries     = 0;
      uint64_t numberOfPuts           = 0;
      uint64_t numberOfCollisions     = 0;
      uint64_t numberOfOverwrites     = 0;
      uint64_t numberOfUpdates        = 0;
      uint64_t numberOfProbes         = 0;
      uint64_t numberOfHits           = 0; // entries with identical key found
      uint64_t numberOfMisses         = 0; // no entry with key found
    };

    mutable std::array<Stats, MAX_SEARCH_THREADS> statsSlots{};

    /// Aggregates statistics across all active thread slots.
    /// Called only on cold paths (str(), hashFull(), getters).
    [[nodiscard]] Stats aggregateStats() const {
      Stats total{};
      const int count = numSmpThreads > 0 ? numSmpThreads : 1;
      for (int i = 0; i < count; ++i) {
        total.numberOfEntries    += statsSlots[i].numberOfEntries;
        total.numberOfPuts       += statsSlots[i].numberOfPuts;
        total.numberOfCollisions += statsSlots[i].numberOfCollisions;
        total.numberOfOverwrites += statsSlots[i].numberOfOverwrites;
        total.numberOfUpdates    += statsSlots[i].numberOfUpdates;
        total.numberOfProbes     += statsSlots[i].numberOfProbes;
        total.numberOfHits       += statsSlots[i].numberOfHits;
        total.numberOfMisses     += statsSlots[i].numberOfMisses;
      }
      return total;
    }

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
    /// If set to 0 MB, TT will ensure at least 1 cluster (4 entries).
    /// @param newSizeInMByte  Size in megabytes, reduced to next lowest power of 2.
    ///                        Limited to 32,768 MB.
    void resize(uint64_t newSizeInMByte);

    /// Clears the transposition table by resetting all entries to zero.
    void clear();

    /// Stores a position in the transposition table.
    /// The move will be stripped of any sort value before storing, as value
    /// is stored separately. This avoids surprising behavior where MOVE_NONE
    /// might appear to have a value.
    /// @param key        Position key (usually Zobrist key)
    /// @param depth      Search depth (0 to DEPTH_MAX, usually 127)
    /// @param move       Best move of the node (for BETA: best move until cutoff)
    /// @param value      Search value between VALUE_MIN and VALUE_MAX
    /// @param type       Value bound type: EXACT, ALPHA, or BETA
    /// @param eval       Static evaluation of the position
    /// @param threadIdx  Thread index for per-thread statistics (default 0)
    void put(ZobristKey key, Depth depth, Move move, Value value, ValueType type, Value eval, int threadIdx = 0);

    /// Retrieves an entry matching the given key without updating statistics.
    /// Scans all entries in the cluster for a match using XOR verification.
    /// Returns a copy for thread safety.
    /// @param key  Position key (usually Zobrist key)
    /// @return     Copy of matching entry, or nullopt if not found
    std::optional<Entry> getMatch(const ZobristKey key) const {
      const TTCluster* const cluster = getClusterConst(key);
      for (int i = 0; i < CLUSTER_SIZE; ++i) {
        const ZobristKey storedKey = cluster->entries[i].key.load(std::memory_order_acquire);
        Entry copy                 = cluster->entries[i]; // Copy via copy constructor
        // Verify: storedKey XOR'd with data hash must equal the probe key.
        if ((storedKey ^ copy.dataHash()) == key) {
          copy.key.store(key, std::memory_order_relaxed); // Restore original key
          return copy;
        }
      }
      return std::nullopt;
    }

    /// Probes the TT for an entry matching the key.
    /// Updates hit/miss statistics. Decreases age of found entry (single-thread mode only;
    /// skipped under SMP to avoid a data race on the packed bitfield byte).
    /// Returns a copy of the entry to ensure thread-safety (caller can safely read
    /// the returned data even if another thread overwrites the original entry).
    /// @param key        Position key (usually Zobrist key)
    /// @param threadIdx  Thread index for per-thread statistics (default 0)
    /// @return           Copy of matching entry, or nullopt if not found
    std::optional<Entry> probe(const ZobristKey& key, int threadIdx = 0);

    /// Ages all entries by incrementing their age counter.
    /// Called at the start of each new search to help with replacement.
    void ageEntries();

    /// Returns how full the transposition table is in permill (0-1000).
    /// Used for UCI "hashfull" info output.
    /// @return  Fill level in permill
    [[nodiscard]] int hashFull() const {
      const std::size_t maxEntries = maxNumberOfClusters * CLUSTER_SIZE;
      return static_cast<int>((1000 * aggregateStats().numberOfEntries) / maxEntries);
    };

    /// Prefetches the TT cluster for the given key into the CPU cache.
    ///
    /// Call this as early as possible before probe(), ideally with other work
    /// in between (e.g., move generation, evaluation setup) to give the memory
    /// subsystem time to fetch the data. The prefetch is asynchronous and does
    /// not block execution. Optimal timing is 100-300 cycles before the actual
    /// memory access, depending on memory latency.
    ///
    /// Uses _MM_HINT_T0 which fetches into all cache levels (L1, L2, L3).
    /// With the bucket design, a single prefetch loads all 4 entries in the
    /// cluster (64 bytes = 1 cache line), significantly improving probe()
    /// performance by hiding memory latency.
    ///
    /// @param key  Position key to prefetch
    void prefetch(const ZobristKey key) const {
#ifdef TT_ENABLE_PREFETCH
      _mm_prefetch(reinterpret_cast<const char*>(&_data[key & clusterMask]), _MM_HINT_T0);
#else
      (void) key;
#endif
    }

    /// Returns a string representation of the TT instance for debugging.
    /// @return  Debug string with size and statistics
    std::string str() const;

  private:
    /// Returns the cluster index from the position key using bitmask.
    /// @param key  Position key
    /// @return     Array index for the cluster
    std::size_t getClusterIndex(const ZobristKey key) const {
      return key & clusterMask;
    }

    /// Returns a mutable pointer to the cluster for the given key.
    /// @param key  Position key
    /// @return     Pointer to the cluster
    TTCluster* getCluster(const ZobristKey key) const {
      return &_data[getClusterIndex(key)];
    }

    /// Returns a const pointer to the cluster for the given key.
    /// @param key  Position key
    /// @return     Const pointer to the cluster
    const TTCluster* getClusterConst(const ZobristKey key) const {
      return &_data[getClusterIndex(key)];
    }

  public:
    // === Getters ===

    /// Returns the size of the TT in bytes.
    uint64_t getSizeInByte() const { return sizeInByte; }

    /// Returns the maximum number of entries the TT can hold (clusters × CLUSTER_SIZE).
    std::size_t getMaxNumberOfEntries() const { return maxNumberOfClusters * CLUSTER_SIZE; }

    /// Returns the number of clusters in the TT.
    std::size_t getMaxNumberOfClusters() const { return maxNumberOfClusters; }

    /// Returns the current number of entries stored.
    std::size_t getNumberOfEntries() const { return aggregateStats().numberOfEntries; }

    /// Returns the total number of put() calls.
    uint64_t getNumberOfPuts() const { return aggregateStats().numberOfPuts; }

    /// Returns the number of hash collisions (different position, same slot).
    uint64_t getNumberOfCollisions() const { return aggregateStats().numberOfCollisions; }

    /// Returns the number of overwrites (replaced existing entry).
    uint64_t getNumberOfOverwrites() const { return aggregateStats().numberOfOverwrites; }

    /// Returns the number of updates (same position, updated entry).
    uint64_t getNumberOfUpdates() const { return aggregateStats().numberOfUpdates; }

    /// Returns the total number of probe() calls.
    uint64_t getNumberOfProbes() const { return aggregateStats().numberOfProbes; }

    /// Returns the number of successful probes (entry with matching key found).
    uint64_t getNumberOfHits() const { return aggregateStats().numberOfHits; }

    /// Returns the number of failed probes (no matching entry found).
    uint64_t getNumberOfMisses() const { return aggregateStats().numberOfMisses; }

    /// Returns the number of threads used for clearing.
    unsigned int getThreads() const { return noOfThreads; }

    /// Sets the number of threads used for clearing.
    /// @param threads  Number of threads (minimum 1)
    void setThreads(const int threads) { noOfThreads = threads > 0 ? static_cast<unsigned int>(threads) : 1U; }

    /// Sets the number of active SMP search threads.
    /// When > 1, probe() skips age-- to avoid a data race on the packed bitfield byte.
    /// Call before each search when thread count changes.
    /// @param threads  Total search threads (1 = single-thread mode, full original behavior)
    void setSmpThreads(const int threads) { numSmpThreads = threads > 0 ? threads : 1; }

    /// Returns the current SMP thread count setting.
    int getSmpThreads() const { return numSmpThreads; }

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

    FRIEND_TEST_NS(TT_Test, put);
    FRIEND_TEST_NS(TT_Test, get);
    FRIEND_TEST_NS(TT_Test, probe);
  };

} // namespace engine

#endif // FRANKYCPP_TT_H
