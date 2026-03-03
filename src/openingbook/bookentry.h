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

#ifndef FRANKYCPP_BOOKENTRY_H
#define FRANKYCPP_BOOKENTRY_H

//=============================================================================
// bookentry.h - Opening Book Entry Data Structure
//=============================================================================
//
// Represents a single entry in the opening book. Each entry corresponds to
// a position and stores the moves that can be played from that position.
// Depends on: types.h (for ZobristKey, Move)
//
// Structure:
//   - key: Zobrist hash of the position for lookup
//   - counter: How often this position appears in the book source
//   - moves: Available moves from this position
//   - nextPosition: Zobrist keys of resulting positions after each move
//
// Serialization:
//   Uses Boost.Serialization for saving/loading book to binary format.
//   This avoids re-parsing PGN/SAN files on every startup.
//
// Usage:
//   BookEntry entry(position.getZobristKey());
//   entry.moves.push_back(move);
//   entry.nextPosition.push_back(newPositionKey);
//   entry.counter++;
//
//=============================================================================

/// An entry in the opening book representing a position and its available moves.
namespace book {
  using namespace chess;

  class BookEntry {
  public:
    ZobristKey key{};                      ///< Zobrist hash of the position
    int counter{1};                        ///< Number of times this position appears in book source
    std::vector<Move> moves{};             ///< Available moves from this position
    std::vector<ZobristKey> nextPosition{};///< Zobrist keys after each corresponding move

    /// Default constructor (required for Boost serialization).
    BookEntry() = default;

    /// Creates an entry for a position with the given Zobrist key.
    /// @param zobrist  Zobrist hash of the position
    explicit BookEntry(const ZobristKey zobrist) : key(zobrist) {}

    /// Returns a string representation for debugging.
    /// @return  Debug string with key, counter, and moves
    [[nodiscard]] std::string str() const {
      std::ostringstream os;
      os << this->key << " (" << this->counter << ")"
         << " [ ";
      for (std::size_t i = 0; i < moves.size(); i++) {
        os << this->moves[i].str() << " ";
      }
      os << "] ";
      return os.str();
    }

    // BOOST Serialization
    friend class boost::serialization::access;

    /// Serializes/deserializes the entry for Boost.Serialization.
    template<class Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
      ar& BOOST_SERIALIZATION_NVP(key);
      ar& BOOST_SERIALIZATION_NVP(counter);
      ar& BOOST_SERIALIZATION_NVP(moves);
      ar& BOOST_SERIALIZATION_NVP(nextPosition);
    }
  };

}// namespace book

#endif// FRANKYCPP_BOOKENTRY_H
