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

#include "types/types.h"
#include "gtest/gtest_prod.h"
#include <iosfwd>

#ifndef FRANKYCPP_TT_H
#define FRANKYCPP_TT_H

// pre-fetching of TT entries into CPU caches
#ifdef __GNUC__
#include <emmintrin.h>
#define TT_ENABLE_PREFETCH
#elif _MSC_VER
#include <xmmintrin.h>
#define TT_ENABLE_PREFETCH
#endif

#ifdef TT_ENABLE_PREFETCH
#define TT_PREFETCH tt->prefetch(p.getZobristKey())
#else
#define TT_PREFETCH void(0);
#endif

/**
 * TT implementation using heap memory and simple hash for entries.
 * The number of entries are always a power of two fitting into the given size.
 * It is not yet thread safe as it has no synchronization.
 *
 * Tests have shown that an implementation with a struct and bitfields is
 * more efficient than using only one 64-bit data field with manual bit shifting
 * and masking (~9% slower)
 * Also using buckets has not shown significant strength improvements and is
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
    // sorted by size to achieve smallest struct size
    // using bitfield for smallest size
    Key key       = 0;         // 64 bit
    uint16_t move = MOVE_NONE; // 16 bit
    Value eval    = VALUE_NONE;// 16 bit signed
    Value value   = VALUE_NONE;// 16 bit signed
    Depth depth : 7;           // 0-127
    uint8_t age : 3;           // 0-7
    ValueType type : 2;        // 4 values
    bool mateThreat : 1;       // 1-bit bool
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

  // this array hold the actual entries for the transposition table
  Entry* _data{};

public:
  // TT default size is 2 MB
  TT() : TT(DEFAULT_TT_SIZE) {}

  /**
   * @param newSizeInMByte Size of TT in bytes which will be reduced to the next lowest power of 2 size
   *                        Limited to 32.000MB
   */
  explicit TT(uint64_t newSizeInMByte);

  ~TT() {
    delete[] _data;
  }

  // disallow copies
  TT(TT const& tt) = delete;         // copy
  TT& operator=(const TT&) = delete; // copy assignment
  TT(TT const&& tt)        = delete; // move
  TT& operator=(const TT&&) = delete;// move assignment

  /**
   * Changes the size of the transposition table and clears all entries.
   * @param newSizeInMByte in Byte which will be reduced to the next
   * lowest power of 2 size. Limited to 32.000 MB.
   */
  void resize(uint64_t newSizeInMByte);

  /** Clears the transposition table be resetting all entries to 0. */
  void clear();

  /**
    * Stores the node value and the depth it has been calculated at.
    * Also stores the best move for the node.
    * OBS: move will be stripped of any value before storing as we store value
    * separately and it may be surprising that a MOVE_NONE has a value.
    * @param key Position key (usually Zobrist key)
    * @param depth 0-DEPTH_MAX (usually 127)
    * @param move best move of the node (when BETA best move until cut off)
    * @param value Value of the position between VALUE_MIN and VALUE_MAX
    * @param type EXACT, ALPHA or BETA
    * @param mateThreat node had a mate threat in the ply
    */
  void put(Key key, Depth depth, Move move, Value value, ValueType type, Value eval, bool mateThreat);

  /**
   * This retrieves a ptr to the entry of this node from cache.
   *
   * @param key Position key (usually Zobrist key)
   * @return Pointer to entry for key or nullptr if not found
   */
  inline const TT::Entry* getMatch(const Key key) const {
    const Entry* const entryPtr = getEntryPtr(key);
    return entryPtr->key == key ? entryPtr : nullptr;
  }

  /**
   * Looks up and returns a pointer to an TT Entry. Decreases age of the entry
   * if an entry was found
   */
  const TT::Entry* probe(const Key& key);

  /** Age all entries by 1 */
  void ageEntries();

  /** Returns how full the transposition table is in permill as per UCI */
  inline int hashFull() const {
    if (!maxNumberOfEntries) return 0;
    return static_cast<int>((1000 * numberOfEntries) / maxNumberOfEntries);
  };

    // using prefetch improves probe lookup speed significantly
#ifdef TT_ENABLE_PREFETCH
  inline void prefetch(const Key key) {
#ifdef __GNUC__
    _mm_prefetch(&_data[(key & hashKeyMask)], _MM_HINT_T0);
#elif _MSC_VER
    _mm_prefetch(reinterpret_cast<const CHAR*>(&_data[(key & hashKeyMask)]), _MM_HINT_T0);
#endif
  }
#endif

  /** return a string representation of the TT instance */
  std::string str();

private:
  /* generates the index hash key from the position key  */
  inline std::size_t getHash(const Key key) const {
    return key & hashKeyMask;
  }

  /* This retrieves a direct pointer to the entry of this node from cache */
  inline TT::Entry* getEntryPtr(const Key key) const {
    return &_data[getHash(key)];
  }

  /** GETTER and SETTER */
public:
  uint64_t getSizeInByte() const {
    return sizeInByte;
  }

  std::size_t getMaxNumberOfEntries() const {
    return maxNumberOfEntries;
  }

  std::size_t getNumberOfEntries() const {
    return numberOfEntries;
  }

  uint64_t getNumberOfPuts() const {
    return numberOfPuts;
  }

  uint64_t getNumberOfCollisions() const {
    return numberOfCollisions;
  }

  uint64_t getNumberOfOverwrites() const {
    return numberOfOverwrites;
  }

  uint64_t getNumberOfUpdates() const {
    return numberOfUpdates;
  }

  uint64_t getNumberOfProbes() const {
    return numberOfProbes;
  }

  uint64_t getNumberOfHits() const {
    return numberOfHits;
  }

  uint64_t getNumberOfMisses() const {
    return numberOfMisses;
  }

  int getThreads() const {
    return noOfThreads;
  }

  void setThreads(int threads) {
    TT::noOfThreads = threads;
  }

  static inline std::string str(const ValueType type) {
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
  FRIEND_TEST(TT_Test, put);
  FRIEND_TEST(TT_Test, get);
  FRIEND_TEST(TT_Test, probe);
};

#endif//FRANKYCPP_TT_H
