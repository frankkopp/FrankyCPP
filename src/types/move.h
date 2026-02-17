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

#ifndef FRANKYCPP_MOVE_H
#define FRANKYCPP_MOVE_H

//=============================================================================
// move.h - Chess Move Representation
//=============================================================================
//
// Move is a 32-bit value encoding a chess move with sorting information.
// Depends on: movetype.h, piecetype.h, value.h
//
// Bit Layout (32 bits):
//
//   Bits  0-5:  To square (0-63)
//   Bits  6-11: From square (0-63)
//   Bits 12-13: Promotion piece type (0-3 → KNIGHT, BISHOP, ROOK, QUEEN)
//   Bits 14-15: Move type (NORMAL, PROMOTION, ENPASSANT, CASTLING)
//   Bits 16-31: Sort value (for move ordering)
//
//   |-value ------------------------|-Move -------------------------|
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 | 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 | 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//   --------------------------------|--------------------------------
//                                   |                     1 1 1 1 1 1  to
//                                   |         1 1 1 1 1 1              from
//                                   |     1 1                          promotion piece type
//                                   | 1 1                              move type
//   1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 |                                  sort value
//
// Special Values:
//   MOVE_NONE = 0    - Invalid / no move
//
// Key Operations:
//   from() / to()         - Source and destination squares
//   moveType()            - NORMAL, PROMOTION, ENPASSANT, CASTLING
//   promotionType()       - Piece type for promotions
//   sortValue()           - Move ordering value
//   stripped()            - Move without sort value (for comparison)
//   str()                 - UCI notation ("e2e4", "e7e8q")
//
// Construction:
//   Move::normal(from, to)
//   Move::promotion(from, to, promType)
//   Move::enPassant(from, to)
//   Move::castling(from, to)
//
// Usage:
//   Move m = Move::normal(SQ_E2, SQ_E4);
//   Square from = m.from();              // SQ_E2
//   std::string uci = m.str();           // "e2e4"
//
//   Move promo = Move::promotion(SQ_E7, SQ_E8, QUEEN);
//   PieceType pt = promo.promotionType(); // QUEEN
//
//=============================================================================

#include "movetype.h"
#include "piecetype.h"
#include "value.h"

#include <boost/serialization/nvp.hpp>
#include <boost/serialization/split_free.hpp>
#include <cassert>
#include <format>
#include <string>

class Move {
public:
  using Raw = uint32_t;// fixed width for stable serialization and cache compatibility

private:
  Raw raw_{0};
  static_assert(sizeof(Raw) == 4, "Move::Raw must remain 32-bit");

  // Internal helper to encode promotion type (expects promType already >= KNIGHT)
  static constexpr Raw encode(const Square from, const Square to, const MoveType mt, const PieceType promType, const Value v) {
    // clang-format off
    return static_cast<Raw>(to)
        | static_cast<Raw>(from) << MoveShifts::FROM_SHIFT
        | static_cast<Raw>(promType - KNIGHT) << MoveShifts::PROM_TYPE_SHIFT
        | static_cast<Raw>(mt)
        | static_cast<Raw>(v - VALUE_NONE) << MoveShifts::VALUE_SHIFT;
    // clang-format on
  }

public:
  // ---------------------------------------------------------------------------
  // Construction
  // ---------------------------------------------------------------------------
  constexpr Move() = default;// MOVE_NONE
  constexpr explicit Move(const Raw raw) : raw_{raw} {}

  // Canonical full constructor (internal anchor).
  constexpr Move(const Square from, const Square to,
                 const MoveType mt, const PieceType promType,
                 const Value v = VALUE_NONE)
      : raw_{encode(from, to, mt, (promType < KNIGHT ? KNIGHT : promType), v)} {
    assert(from.isValid() && to.isValid());
  }

  // Convenience reduced forms.
  constexpr Move(const Square from, const Square to)
      : Move(from, to, NORMAL, KNIGHT, VALUE_NONE) {}

  constexpr Move(const Square from, const Square to,
                 const MoveType mt, const Value v)
      : Move(from, to, mt, KNIGHT, v) {}

  // Named helpers (clearer at call sites).
  static constexpr Move normal(const Square from, const Square to,
                               const Value v = VALUE_NONE) {
    return Move(from, to, NORMAL, KNIGHT, v);
  }
  static constexpr Move enPassant(const Square from, const Square to,
                                  const Value v = VALUE_NONE) {
    return Move(from, to, ENPASSANT, KNIGHT, v);
  }
  static constexpr Move castling(const Square from, const Square to,
                                 const Value v = VALUE_NONE) {
    return Move(from, to, CASTLING, KNIGHT, v);
  }
  static constexpr Move promotion(const Square from, const Square to,
                                  const PieceType promo, const Value v = VALUE_NONE) {
    return Move(from, to, PROMOTION, promo, v);
  }

  // Access to the underlying raw integer (for low-level / legacy uses).
  constexpr Raw raw() const noexcept { return raw_; }

// True if this is MOVE_NONE (no encoded move bits).
  constexpr bool isNone() const noexcept { return (raw_ & MoveShifts::MOVE_MASK) == 0; }

  // True if a non-default (scoring/sorting) value is attached.
  constexpr bool hasValue() const noexcept { return !isNone() && value() != VALUE_NONE; }

  // Encoded source square.
  constexpr Square from() const noexcept {
    return static_cast<Square>((raw_ & MoveShifts::FROM_MASK) >> MoveShifts::FROM_SHIFT);
  }

  // Encoded destination square.
  constexpr Square to() const noexcept {
    return static_cast<Square>(raw_ & MoveShifts::TO_MASK);
  }

  // Encoded move type (normal, promotion, en-passant, castling).
  constexpr MoveType type() const noexcept {
    return static_cast<MoveType>(raw_ & MoveShifts::MOVE_TYPE_MASK);
  }

  // Encoded promotion piece type (only meaningful for promotion moves).
  constexpr PieceType promotionType() const noexcept {
    return static_cast<PieceType>(((raw_ & MoveShifts::PROM_TYPE_MASK) >> MoveShifts::PROM_TYPE_SHIFT) + KNIGHT);
  }

  // Attached (sorting/evaluation) value or VALUE_NONE if none.
  constexpr Value value() const noexcept {
    return Value{static_cast<int>((raw_ & MoveShifts::VALUE_MASK) >> MoveShifts::VALUE_SHIFT)} + VALUE_NONE;
  }

  // Returns a Move with only the move bits (value cleared).
  constexpr Move stripped() const noexcept { return Move{(raw_ & MoveShifts::MOVE_MASK)}; }

  // Encodes a (sorting) value into this move (mutating). No-op for MOVE_NONE.
  constexpr void setValue(const Value v) noexcept {
    if (isNone()) return;
    raw_ = raw_ & MoveShifts::MOVE_MASK | static_cast<Raw>(v - VALUE_NONE) << MoveShifts::VALUE_SHIFT;
  }
// Valid if not MOVE_NONE and all encoded subfields (promotion type, move type, value) are within allowed ranges
  constexpr bool isValid() const noexcept {
    return !isNone() && validPieceType(promotionType()) && validMoveType(type()) && (value() == VALUE_NONE || value().isValid());
  }

  // String (UCI style)
  std::string str() const {
    if (stripped().isNone()) return "no move";
    if (type() == PROMOTION) return from().str() + to().str() + ::str(promotionType());
    return from().str() + to().str();
  }

  // Verbose representation
  std::string strVerbose() const {
    if (isNone()) return "no move 0";// maintain similar behavior
    std::string tp;
    std::string promPt;
    switch (type()) {
      case NORMAL:
        tp = "n";
        break;
      case PROMOTION:
        promPt = ::str(promotionType());
        tp     = "p";
        break;
      case ENPASSANT:
        tp = "e";
        break;
      case CASTLING:
        tp = "c";
        break;
    }
    return std::format("Move: {:2}{:2}{:1}  type:{:<1}  prom:{:<1}  value:{:<6}  ({})",
                       from().str(), to().str(), promPt, tp, promPt,
                       std::to_string(value()), std::to_string(raw_));
  }

  // Comparison by raw integer (fast).
  constexpr bool operator==(const Move&) const noexcept = default;
  constexpr bool operator!=(const Move&) const noexcept = default;
  constexpr auto operator<=>(const Move&) const noexcept = default;

  // Allow implicit conversion to raw for legacy bitwise operations & masks.
  // (If stricter encapsulation is desired later, make this explicit and adapt call sites.)
  // ReSharper disable once CppNonExplicitConversionOperator
  constexpr operator Raw() const noexcept { return raw_; }

  // Removed Boost member serialize to allow primitive/bitwise treatment for backward-compatible raw storage.
};

// Global constant for no-move (replaces enumerator MOVE_NONE from old enum). Kept name for compatibility.
inline constexpr Move MOVE_NONE{};

ENABLE_OSTREAM_OPERATOR_AS_STR_ON(Move)

// Comparator updated to use raw() while remaining trivially inline.
struct moveValueGreaterComparator {
  constexpr bool operator()(const Move lhs, const Move rhs) const {
    return (lhs.raw() & MoveShifts::VALUE_MASK) > (rhs.raw() & MoveShifts::VALUE_MASK);
  }
};

// Boost split-free serialization: persists Move as its 32-bit raw value for backward compatibility.
namespace boost::serialization {
  template<class Archive>
  void save(Archive& ar, const Move& m, const unsigned int /*version*/) {
    Move::Raw r = m.raw();
    ar& make_nvp("raw", r);
  }
  template<class Archive>
  void load(Archive& ar, Move& m, const unsigned int /*version*/) {
    Move::Raw r{};
    ar& make_nvp("raw", r);
    m = Move(r);
  }
  template<class Archive>
  void serialize(Archive& ar, Move& m, const unsigned int version) {
    serialization::split_free(ar, m, version);
  }
}// namespace boost::serialization

#endif// FRANKYCPP_MOVE_H
