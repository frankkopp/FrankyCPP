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
#include "types/bitboards.h"// for Bitboards::pawnAttacks

#include <algorithm>// for std::replace

// Fathom C library header
extern "C" {
#include "tbprobe.h"
}

//=============================================================================
// Static assertions to verify FrankyCPP square encoding matches Fathom's expectations
// Fathom uses Little-Endian Rank-File mapping: A1=0, H1=7, A8=56, H8=63
//=============================================================================
static_assert(static_cast<int>(SQ_A1) == 0, "Square A1 must be 0 for Fathom compatibility");
static_assert(static_cast<int>(SQ_H1) == 7, "Square H1 must be 7 for Fathom compatibility");
static_assert(static_cast<int>(SQ_A8) == 56, "Square A8 must be 56 for Fathom compatibility");
static_assert(static_cast<int>(SQ_H8) == 63, "Square H8 must be 63 for Fathom compatibility");
static_assert(static_cast<int>(SQ_NONE) == 64, "SQ_NONE must be 64 (Fathom uses 0 for no en passant)");

// Verify en passant squares match Fathom's expected range
// Fathom expects EP squares on rank 3 (16-23) for black pawns or rank 6 (40-47) for white pawns
static_assert(static_cast<int>(SQ_A3) == 16, "Square A3 must be 16 for Fathom EP compatibility");
static_assert(static_cast<int>(SQ_H3) == 23, "Square H3 must be 23 for Fathom EP compatibility");
static_assert(static_cast<int>(SQ_A6) == 40, "Square A6 must be 40 for Fathom EP compatibility");
static_assert(static_cast<int>(SQ_H6) == 47, "Square H6 must be 47 for Fathom EP compatibility");

namespace tablebase {

  namespace {

    // TB score constants - high values but below checkmate threshold
    // These values indicate a known win/loss from tablebase
    constexpr auto TB_WIN_SCORE  = Value{9000};// Below VALUE_MAX (10000) to leave room for mate scores
    constexpr auto TB_LOSS_SCORE = Value{-9000};

    /// Convert FrankyCPP Position to Fathom's required bitboard format
    /// @return Total piece count
    /// Note: Fathom uses the same square mapping as FrankyCPP (A1=0, H1=7, A8=56, H8=63)
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
      kings   = pos.getPieceBb(WHITE, KING) | pos.getPieceBb(BLACK, KING);
      queens  = pos.getPieceBb(WHITE, QUEEN) | pos.getPieceBb(BLACK, QUEEN);
      rooks   = pos.getPieceBb(WHITE, ROOK) | pos.getPieceBb(BLACK, ROOK);
      bishops = pos.getPieceBb(WHITE, BISHOP) | pos.getPieceBb(BLACK, BISHOP);
      knights = pos.getPieceBb(WHITE, KNIGHT) | pos.getPieceBb(BLACK, KNIGHT);
      pawns   = pos.getPieceBb(WHITE, PAWN) | pos.getPieceBb(BLACK, PAWN);

      // En passant square handling:
      // Fathom requires ep=0 unless an en passant capture is actually LEGAL for the side to move.
      // Simply having an EP square set isn't enough - there must be a pawn that can capture.
      ep                = 0;
      const Square epSq = pos.getEnPassantSquare();
      if (epSq != SQ_NONE) [[unlikely]] {
        const Color stm         = pos.getNextPlayer();
        const Bitboard stmPawns = pos.getPieceBb(stm, PAWN);
        // Get squares that can attack the EP square (i.e., pawns that could capture there)
        // pawnAttacks[enemy_color][epSq] gives squares from which OUR pawns could capture TO epSq
        if (Bitboards::pawnAttacks[~stm][epSq] & stmPawns) {
          ep = static_cast<unsigned>(epSq);
        }
      }

      // Side to move (Fathom: true = white to move)
      turn = pos.getNextPlayer() == WHITE;

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
      // Fathom uses the same square mapping as FrankyCPP (A1=0, H1=7, A8=56, H8=63)
      const auto from      = static_cast<Square>(TB_GET_FROM(fathomMove));
      const auto to        = static_cast<Square>(TB_GET_TO(fathomMove));
      const unsigned promo = TB_GET_PROMOTES(fathomMove);

      // Validate squares are within valid range (0-63)
      if (from >= SQ_NONE || to >= SQ_NONE) {
        LOG__WARN(Logger::get().TB_LOG, "Tablebase: Invalid move squares from={} to={}",
                  static_cast<int>(from), static_cast<int>(to));
        return MOVE_NONE;
      }

      // Get the piece on the from square for move type detection
      const Piece movingPiece = pos.getPiece(from);

      // Defensive check: verify there's actually a piece on the from square
      if (movingPiece == PIECE_NONE) {
        LOG__WARN(Logger::get().TB_LOG, "Tablebase: No piece on from square {} in position {}",
                  from.str(), pos.strFen());
        return MOVE_NONE;
      }

      // Handle promotions
      if (promo != TB_PROMOTES_NONE) {
        // Verify it's actually a pawn moving
        if (typeOf(movingPiece) != PAWN) {
          LOG__WARN(Logger::get().TB_LOG, "Tablebase: Promotion move but piece is '{}' not pawn",
                    str(movingPiece));
        }
        PieceType pt{};
        switch (promo) {
          case TB_PROMOTES_QUEEN:
            pt = QUEEN;
            break;
          case TB_PROMOTES_ROOK:
            pt = ROOK;
            break;
          case TB_PROMOTES_BISHOP:
            pt = BISHOP;
            break;
          case TB_PROMOTES_KNIGHT:
            pt = KNIGHT;
            break;
          default:
            LOG__WARN(Logger::get().TB_LOG, "Tablebase: Unknown promotion type {}", promo);
            pt = QUEEN;
            break;
        }
        return Move::promotion(from, to, pt);
      }

      // Handle en passant - pawn moving to the en passant square
      if (typeOf(movingPiece) == PAWN && to == pos.getEnPassantSquare()) {
        return Move::enPassant(from, to);
      }

      // Handle castling (king moves 2 squares horizontally)
      if (typeOf(movingPiece) == KING) {
        const int fileDiff = static_cast<int>(to.file()) - static_cast<int>(from.file());
        if (fileDiff == 2 || fileDiff == -2) {
          return Move::castling(from, to);
        }
      }

      // Normal move (including captures)
      return Move::normal(from, to);
    }

    /// Convert Fathom WDL value to our TBResult enum
    TBResult convertWDL(const unsigned wdl) {
      switch (wdl) {
        case TB_WIN:
          return TBResult::Win;
        case TB_CURSED_WIN:
          return TBResult::CursedWin;
        case TB_DRAW:
          return TBResult::Draw;
        case TB_BLESSED_LOSS:
          return TBResult::BlessedLoss;
        case TB_LOSS:
          return TBResult::Loss;
        default:
          LOG__WARN(Logger::get().TB_LOG, "Tablebase: Unknown WDL value {} from Fathom", wdl);
          return TBResult::Failed;
      }
    }

  }// anonymous namespace

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

    // Normalize path for the platform (Fathom expects native separators)
    std::string normalizedPath = path;
#ifdef _WIN32
    // Convert forward slashes to backslashes for Windows
    std::ranges::replace(normalizedPath, '/', '\\');
#endif

    LOG__INFO(Logger::get().TB_LOG, "Tablebase: Initializing from path: {}", normalizedPath);

    // Initialize Fathom with the path
    // tb_init returns true even if no files found (TB_LARGEST will be 0)
    // We check TB_LARGEST > 0 to verify tablebases were actually loaded
    tb_init(normalizedPath.c_str());

    if (TB_LARGEST > 0) {
      maxPieces_   = static_cast<int>(TB_LARGEST);
      tbPath_      = normalizedPath;
      initialized_ = true;
      LOG__INFO(Logger::get().TB_LOG, "Tablebase: Initialized successfully, max pieces = {}", maxPieces_);
    }
    else {
      maxPieces_ = 0;
      tbPath_.clear();
      initialized_ = false;
      LOG__WARN(Logger::get().TB_LOG, "Tablebase: No tablebase files found in path: {}", normalizedPath);
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
    return pos.getOccupiedBb().popcount() <= maxPieces_;
  }

  TBResult Tablebase::probeWDL(const Position& pos) const {
    if (!canProbe(pos)) {
      LOG__DEBUG(Logger::get().TB_LOG, "probeWDL: canProbe returned false");
      return TBResult::Failed;
    }

    // Convert position to Fathom format
    uint64_t white{}, black{}, kings{}, queens{}, rooks{}, bishops{}, knights{}, pawns{};
    unsigned ep{};
    bool turn{};
    convertPositionToFathom(pos, white, black, kings, queens, rooks, bishops, knights, pawns, ep, turn);

    // Log all bitboards for debugging
    // LOG__DEBUG(Logger::get().TB_LOG, "probeWDL: position FEN = {}", pos.strFen());
    // LOG__DEBUG(Logger::get().TB_LOG, "probeWDL: white=0x{:016x} black=0x{:016x}", white, black);
    // LOG__DEBUG(Logger::get().TB_LOG, "probeWDL: kings=0x{:016x} queens=0x{:016x}", kings, queens);
    // LOG__DEBUG(Logger::get().TB_LOG, "probeWDL: rooks=0x{:016x} bishops=0x{:016x}", rooks, bishops);
    // LOG__DEBUG(Logger::get().TB_LOG, "probeWDL: knights=0x{:016x} pawns=0x{:016x}", knights, pawns);
    // LOG__DEBUG(Logger::get().TB_LOG, "probeWDL: ep={} turn={}", ep, turn);

    // Probe WDL
    // IMPORTANT: Fathom's tb_probe_wdl REQUIRES rule50=0 and castling=0, otherwise it returns TB_RESULT_FAILED.
    // This is by design - WDL probing gives the "pure" theoretical result without 50-move considerations.
    // For 50-move aware results, use probeRoot, which calls tb_probe_root with the actual halfmove clock.
    const unsigned result = tb_probe_wdl(
      white, black, kings, queens, rooks, bishops, knights, pawns,
      0,// rule50 - MUST be 0 for tb_probe_wdl (Fathom requirement)
      0,// castling - MUST be 0 (we already verify in canProbe)
      ep, turn);

    // LOG__DEBUG(Logger::get().TB_LOG, "probeWDL: tb_probe_wdl returned 0x{:08x}", result);

    if (result == TB_RESULT_FAILED) {
      return TBResult::Failed;
    }

    return convertWDL(result);
  }

  TBProbeResult Tablebase::probeRoot(const Position& pos) const {
    TBProbeResult result;

    if (!canProbe(pos)) {
      return result;// Returns Failed by default
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
      0,// castling - must be 0 (we already check in canProbe)
      ep, turn,
      nullptr// We don't need the full results array
    );

    if (tbResult == TB_RESULT_FAILED) {
      return result;// Returns Failed by default
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
      case TBResult::Win:
        return "Win";
      case TBResult::CursedWin:
        return "Cursed Win";
      case TBResult::Draw:
        return "Draw";
      case TBResult::BlessedLoss:
        return "Blessed Loss";
      case TBResult::Loss:
        return "Loss";
      case TBResult::Failed:
        return "Failed";
      default:
        return "Unknown";
    }
  }

}// namespace tablebase
