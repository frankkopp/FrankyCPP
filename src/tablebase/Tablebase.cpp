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

#include "tablebase/Tablebase.h"
#include "common/Logging.h"

// Fathom C library header
extern "C" {
#include "tbprobe.h"
}

namespace tablebase {

namespace {

// TB score constants - high values but below checkmate threshold
// These values indicate a known win/loss from tablebase
constexpr auto TB_WIN_SCORE  = Value{9000};   // Below VALUE_MAX (10000) to leave room for mate scores
constexpr auto TB_LOSS_SCORE = Value{-9000};

/// Convert FrankyCPP Position to Fathom's required bitboard format
/// @return Total piece count
unsigned convertPositionToFathom(const Position& pos,
                                  uint64_t& white, uint64_t& black,
                                  uint64_t& kings, uint64_t& queens,
                                  uint64_t& rooks, uint64_t& bishops,
                                  uint64_t& knights, uint64_t& pawns,
                                  unsigned& ep, bool& turn) {
  // Get color bitboards (Bitboard has implicit conversion to uint64_t)
  white = pos.getOccupiedBb(WHITE);
  black = pos.getOccupiedBb(BLACK);

  // Get piece type bitboards (combined for both colors)
  kings   = pos.getPieceBb(WHITE, KING)   | pos.getPieceBb(BLACK, KING);
  queens  = pos.getPieceBb(WHITE, QUEEN)  | pos.getPieceBb(BLACK, QUEEN);
  rooks   = pos.getPieceBb(WHITE, ROOK)   | pos.getPieceBb(BLACK, ROOK);
  bishops = pos.getPieceBb(WHITE, BISHOP) | pos.getPieceBb(BLACK, BISHOP);
  knights = pos.getPieceBb(WHITE, KNIGHT) | pos.getPieceBb(BLACK, KNIGHT);
  pawns   = pos.getPieceBb(WHITE, PAWN)   | pos.getPieceBb(BLACK, PAWN);

  // En passant square (Fathom uses 0 for none, we use SQ_NONE)
  const Square epSq = pos.getEnPassantSquare();
  ep = (epSq != SQ_NONE) ? static_cast<unsigned>(epSq) : 0;

  // Side to move (Fathom: true = white to move)
  turn = (pos.getNextPlayer() == WHITE);

  // Return total piece count
  return static_cast<unsigned>(pos.getOccupiedBb().popcount());
}

/// Convert Fathom move encoding to FrankyCPP Move
/// @param fathomMove  Fathom's packed move representation
/// @param pos         Position for context (piece types, en passant)
/// @return FrankyCPP Move or MOVE_NONE if conversion fails
Move convertFathomMove(const unsigned fathomMove, const Position& pos) {
  if (fathomMove == TB_RESULT_FAILED) {
    return MOVE_NONE;
  }

  // Extract move components using Fathom macros
  const auto from  = static_cast<Square>(TB_GET_FROM(fathomMove));
  const auto to    = static_cast<Square>(TB_GET_TO(fathomMove));
  const unsigned promo = TB_GET_PROMOTES(fathomMove);

  // Validate squares
  if (from >= SQ_NONE || to >= SQ_NONE) {
    LOG__WARN(Logger::get().TB_LOG, "Tablebase: Invalid move squares from={} to={}",
              static_cast<int>(from), static_cast<int>(to));
    return MOVE_NONE;
  }

  // Handle promotions
  if (promo != TB_PROMOTES_NONE) {
    PieceType pt{};  // Default promotion piece
    switch (promo) {
      case TB_PROMOTES_QUEEN:  pt = QUEEN;  break;
      case TB_PROMOTES_ROOK:   pt = ROOK;   break;
      case TB_PROMOTES_BISHOP: pt = BISHOP; break;
      case TB_PROMOTES_KNIGHT: pt = KNIGHT; break;
      default:
        LOG__WARN(Logger::get().TB_LOG, "Tablebase: Unknown promotion type {}", promo);
        pt = QUEEN;
        break;
    }
    return Move::promotion(from, to, pt);
  }

  // Handle en passant
  const Piece movingPiece = pos.getPiece(from);
  if (typeOf(movingPiece) == PAWN && to == pos.getEnPassantSquare()) {
    return Move::enPassant(from, to);
  }

  // Handle castling (king moves 2 squares)
  if (typeOf(movingPiece) == KING) {
    const int distance = static_cast<int>(to) - static_cast<int>(from);
    if (distance == 2 || distance == -2) {
      return Move::castling(from, to);
    }
  }

  // Normal move
  return Move::normal(from, to);
}

/// Convert Fathom WDL value to our TBResult enum
TBResult convertWDL(const unsigned wdl) {
  switch (wdl) {
    case TB_WIN:         return TBResult::Win;
    case TB_CURSED_WIN:  return TBResult::CursedWin;
    case TB_DRAW:        return TBResult::Draw;
    case TB_BLESSED_LOSS: return TBResult::BlessedLoss;
    case TB_LOSS:        return TBResult::Loss;
    default:             return TBResult::Failed;
  }
}

} // anonymous namespace

//=============================================================================
// Tablebase Public Methods
//=============================================================================

Tablebase::~Tablebase() {
  shutdown();
}

bool Tablebase::initialize(const std::string& path) {
  if (path.empty()) {
    LOG__INFO(Logger::get().TB_LOG, "Tablebase: No path specified, tablebases disabled");
    return false;
  }

  // Shutdown any previous initialization
  if (initialized_) {
    shutdown();
  }

  LOG__INFO(Logger::get().TB_LOG, "Tablebase: Initializing from path: {}", path);

  // Initialize Fathom with the path
  // tb_init returns true even if no files found (TB_LARGEST will be 0)
  // We check TB_LARGEST > 0 to verify tablebases were actually loaded
  tb_init(path.c_str());

  if (TB_LARGEST > 0) {
    maxPieces_   = static_cast<int>(TB_LARGEST);
    tbPath_      = path;
    initialized_ = true;
    LOG__INFO(Logger::get().TB_LOG, "Tablebase: Initialized successfully, max pieces = {}", maxPieces_);
  } else {
    maxPieces_   = 0;
    tbPath_.clear();
    initialized_ = false;
    LOG__WARN(Logger::get().TB_LOG, "Tablebase: No tablebase files found in path: {}", path);
  }

  return initialized_;
}

void Tablebase::shutdown() {
  if (initialized_) {
    LOG__DEBUG(Logger::get().TB_LOG, "Tablebase: Shutting down");
    tb_free();
    initialized_ = false;
    maxPieces_   = 0;
    tbPath_.clear();
  }
}

bool Tablebase::canProbe(const Position& pos) const {
  if (!isAvailable()) {
    return false;
  }

  // Cannot probe positions with castling rights
  // (Syzygy TBs only cover positions without castling)
  if (pos.getCastlingRights() != NO_CASTLING) {
    return false;
  }

  // Check piece count is within available tablebase range
  const int pieceCount = pos.getOccupiedBb().popcount();
  return pieceCount <= maxPieces_;
}

TBResult Tablebase::probeWDL(const Position& pos) const {
  if (!canProbe(pos)) {
    return TBResult::Failed;
  }

  // Convert position to Fathom format
  uint64_t white{}, black{}, kings{}, queens{}, rooks{}, bishops{}, knights{}, pawns{};
  unsigned ep{};
  bool turn{};
  convertPositionToFathom(pos, white, black, kings, queens, rooks, bishops, knights, pawns, ep, turn);

  // Probe WDL
  // Note: rule50 and castling are passed as 0 (Fathom requires them to be 0 for WDL probe)
  const unsigned result = tb_probe_wdl(
    white, black, kings, queens, rooks, bishops, knights, pawns,
    0,   // rule50 - must be 0 for WDL probe
    0,   // castling - must be 0 (we already check in canProbe)
    ep, turn
  );

  if (result == TB_RESULT_FAILED) {
    return TBResult::Failed;
  }

  return convertWDL(result);
}

TBProbeResult Tablebase::probeRoot(const Position& pos) const {
  TBProbeResult result;

  if (!canProbe(pos)) {
    return result;  // Returns Failed by default
  }

  // Convert position to Fathom format
  uint64_t white{}, black{}, kings{}, queens{}, rooks{}, bishops{}, knights{}, pawns{};
  unsigned ep{};
  bool turn{};
  convertPositionToFathom(pos, white, black, kings, queens, rooks, bishops, knights, pawns, ep, turn);

  // Get half-move clock for DTZ calculation
  const auto rule50 = static_cast<unsigned>(pos.getHalfMoveClock());

  // Probe root - this gives us WDL, DTZ, and best move
  const unsigned tbResult = tb_probe_root(
    white, black, kings, queens, rooks, bishops, knights, pawns,
    rule50,
    0,   // castling - must be 0 (we already check in canProbe)
    ep, turn,
    nullptr  // We don't need the full results array
  );

  if (tbResult == TB_RESULT_FAILED) {
    return result;  // Returns Failed by default
  }

  // Extract WDL
  result.wdl = convertWDL(TB_GET_WDL(tbResult));

  // Extract DTZ
  result.dtz = static_cast<int>(TB_GET_DTZ(tbResult));

  // Extract and convert best move
  result.bestMove = convertFathomMove(tbResult, pos);

  return result;
}

Value Tablebase::tbValueToScore(const TBResult result, const Depth ply) {
  // Convert TB result to centipawn score
  // We subtract ply to prefer shorter wins (like mate scoring)
  switch (result) {
    case TBResult::Win:
      return TB_WIN_SCORE - Value{ply};
    case TBResult::CursedWin:
      // Cursed win = would be win but for 50-move rule
      // Score it slightly below normal win
      return TB_WIN_SCORE - Value{100} - Value{ply};
    case TBResult::Draw:
      return VALUE_DRAW;
    case TBResult::BlessedLoss:
      // Blessed loss = would be loss but for 50-move rule
      // Score it slightly above normal loss
      return TB_LOSS_SCORE + Value{100} + Value{ply};
    case TBResult::Loss:
      return TB_LOSS_SCORE + Value{ply};
    case TBResult::Failed:
    default:
      return VALUE_NONE;
  }
}

std::string Tablebase::resultToString(const TBResult result) {
  switch (result) {
    case TBResult::Win:         return "Win";
    case TBResult::CursedWin:   return "Cursed Win";
    case TBResult::Draw:        return "Draw";
    case TBResult::BlessedLoss: return "Blessed Loss";
    case TBResult::Loss:        return "Loss";
    case TBResult::Failed:      return "Failed";
    default:                    return "Unknown";
  }
}

} // namespace tablebase
