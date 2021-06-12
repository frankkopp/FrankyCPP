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

#ifndef FRANKYCPP_PAWNTT_H
#define FRANKYCPP_PAWNTT_H

#include "types/types.h"

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

/**
 * TT implementation for pawn evaluations using heap memory and simple hash for entries.
 * The number of entries are always a power of two fitting into the given size.
 * It is not yet thread safe as it has no synchronization.
 */
class PawnTT {

public:
  static constexpr int CacheLineSize        = 64;
  static constexpr uint64_t DEFAULT_TT_SIZE = 2;// MByte
  static constexpr uint64_t MAX_SIZE_MB     = 4'096;

  /** Entry struct for the eval cache */
  struct Entry {
    Key key          = 0;
    Value midvalue = VALUE_NONE;
    Value endvalue = VALUE_NONE;

    std::string str() const {
      return fmt::format("id {} midvalue {} endvalue {}", key, midvalue, endvalue);
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
  // this array hold the actual entries for the transposition table
  Entry* _data{};

  // threads for clearing hash
  unsigned int noOfThreads = 1;

  // size and fill info
  uint64_t sizeInByte            = 0;
  std::size_t maxNumberOfEntries = 0;
  std::size_t hashKeyMask        = 0;
  std::size_t numberOfEntries    = 0;

  mutable uint64_t numberOfQueries   = 0;
  mutable uint64_t numberOfHits    = 0;// entries with identical key found
  mutable uint64_t numberOfMisses  = 0;// no entry with key found

  mutable uint64_t numberOfPuts       = 0;
  mutable uint64_t numberOfCollisions = 0;
  mutable uint64_t numberOfOverwrites = 0;
  mutable uint64_t numberOfUpdates = 0;

public:
  // TT default size is 2 MB
  PawnTT() : PawnTT(DEFAULT_TT_SIZE) {}

  // newSizeInMByte Size of TT in bytes which will be reduced to the next lowest power of 2 size
  explicit PawnTT(uint64_t newSizeInMByte);

  ~PawnTT() {
    delete[] _data;
  }

  // disallow copies
  PawnTT(PawnTT const& tt) = delete;         // copy
  PawnTT& operator=(const PawnTT&) = delete; // copy assignment
  PawnTT(PawnTT const&& tt)        = delete; // move
  PawnTT& operator=(const PawnTT&&) = delete;// move assignment

  /**
 * Changes the size of the transposition table and clears all entries.
 * @param newSizeInMByte in Byte which will be reduced to the next
 * lowest power of 2 size. Limited to 32.000 MB.
 */
  void resize(uint64_t newSizeInMByte);

  /** Clears the transposition table be resetting all entries to 0. */
  void clear();

  // putEntry stores a Score for a pawn structure represented by the
  // pawn zobrist key in the cache. As usually a query happens before this
  // we can expect that the pointer to the entry is already known
  // and can be provided. Otherwise a call to getEntryPtr is necessary.
  void put(Entry* entryPtr, Key key, Score score);

  /* This retrieves a direct pointer to the entry of this node from cache */
  inline Entry* getEntryPtr(const Key key) const {
    return &_data[getHash(key)];
  }

  /* generates the index hash key from the position key  */
  inline std::size_t getHash(const Key key) const {
    return key & hashKeyMask;
  }

  // return a string representation of the TT instance
  std::string str();

  // using prefetch improves probe lookup speed significantly
#ifdef EVAL_ENABLE_PREFETCH
  inline void prefetch(const Key key) {
#ifdef __GNUC__
    _mm_prefetch(&_data[(key & hashKeyMask)], _MM_HINT_T0);
#elif _MSC_VER
    _mm_prefetch((reinterpret_cast<const char*>(&_data[(key & hashKeyMask)])), _MM_HINT_T0);
#endif
  }
#endif

private:


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

  uint64_t getNumberOfHits() const {
    return numberOfHits;
  }

  uint64_t getNumberOfMisses() const {
    return numberOfMisses;
  }

  uint64_t getNumberOfUpdates() const {
    return numberOfUpdates;
  }

  uint64_t getNumberOfPuts() const {
    return numberOfPuts;
  }

  uint64_t getNumberOfCollisions() const {
    return numberOfCollisions;
  }
};

#endif//FRANKYCPP_PAWNTT_H
