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

#ifndef FRANKYCPP_MOVEGENERATOR_H
#define FRANKYCPP_MOVEGENERATOR_H

//=============================================================================
// MoveGenerator.h - Chess Move Generation
//=============================================================================
//
// MoveGenerator contains functionality to generate moves for a chess position.
// It implements several variants: pseudo-legal moves, legal moves, and
// on-demand (phased) generation of pseudo-legal moves.
// Depends on: types.h, Position, History
//
// Generation Modes (GenMode):
//   GenZero     = 0b00  - No moves
//   GenNonQuiet = 0b01  - Captures and promotions only
//   GenQuiet    = 0b10  - Non-capturing moves only
//   GenAll      = 0b11  - All moves
//
// Ply Dependency:
//   A MoveGenerator instance is bound to a specific ply because it stores
//   PV moves and killer moves which are ply-specific. Usually one MoveGenerator
//   is pre-created for each possible ply to be reused during search.
//
// Memory Management:
//   The constructor is the only time memory is allocated. The instance reuses
//   internal lists for all subsequent move generation calls. Be careful when
//   storing generated move lists - the underlying list will change on the next
//   generation call. A deep copy is necessary if you need to preserve moves.
//
// Move Ordering:
//   1. PV move (if set via setPV())
//   2. Captures (MVV-LVA ordering)
//   3. Killer moves (as soon as generated)
//   4. Quiet moves (history heuristic ordering)
//
// Evasion Mode:
//   When the king is in check, evasion mode generates only moves that can
//   potentially escape check. This significantly reduces the number of moves
//   generated. However, some non-legal moves may still be included because
//   fully calculating all scenarios (e.g., discovered checks, pins) would be
//   more expensive than simply generating the move and dismissing it later
//   when legality is checked. Due to beta cutoffs, we often never need to
//   check full legality of all moves anyway.
//
// Key Methods:
//   generatePseudoLegalMoves()  - Generate all pseudo-legal moves
//   generateLegalMoves()        - Generate all legal moves (expensive)
//   getNextPseudoLegalMove()    - Phased generation for search
//   hasLegalMove()              - Quick check if any legal move exists
//   getMoveFromUci/San()        - Parse move strings
//
// Usage:
//   MoveGenerator mg;
//   mg.setPV(ttMove);
//   mg.storeKiller(killerMove);
//   Move move;
//   while ((move = mg.getNextPseudoLegalMove(pos, GenAll, inCheck)) != MOVE_NONE) {
//     if (!pos.isLegalMove(move)) continue;
//     // process move
//   }
//
//=============================================================================

#include "common/gtest_friends.h"
#include <types/types.h>

// forward declaration
class Position;
struct History;

/// Generation modes for move generation.
/// Can be combined as bit flags: GenAll = GenNonQuiet | GenQuiet.
enum GenMode : uint8_t {
  GenZero     = 0b00,  ///< No moves
  GenNonQuiet = 0b01,  ///< Captures and promotions only
  GenQuiet    = 0b10,  ///< Non-capturing moves only
  GenAll      = 0b11   ///< All moves (captures + quiet)
};

class MoveGenerator {
  // internal move lists to not allocate memory during move generation
  MoveList pseudoLegalMoves = MoveList{};
  MoveList legalMoves       = MoveList{};
  MoveList onDemandMoves    = MoveList{};

  // States for the on demand move generator
  ZobristKey currentODZobrist            = 0;
  Bitboard onDemandEvasionTargets = BbZero;
  std::size_t takeIndex           = 0;
  enum onDemandStage {
    OD_NEW,
    PV_MOVE,
    PAWN_CAPTURES,
    OFFICER_CAPTURES,
    KING_CAPTURES,
    QUIET_SWITCH,
    PAWN_MOVES,
    CASTLING_MOVES,
    OFFICER_MOVES,
    KING_MOVES,
    OD_END
  };
  onDemandStage currentODStage;

  Move pvMove          = MOVE_NONE;
  bool pvMovePush    = false;
  std::array<Move, 2> killerMoves  = {MOVE_NONE, MOVE_NONE};
  History* historyData = nullptr;

public:
  /// Creates a new MoveGenerator instance.
  /// This is the only time memory is allocated. The instance will not create
  /// any new move lists during normal move generation as it reuses pre-created
  /// internal lists which are returned via pointer to the caller.
  /// Note: Be careful when trying to store the list of generated moves as the
  /// underlying list will be changed when move generation is called again.
  /// A deep copy is necessary if you need to preserve the move list.
  MoveGenerator();

  ~MoveGenerator() = default;

  /// Generates pseudo-legal moves for the next player.
  /// Does not check if king is left in check or if it passes an attacked square
  /// when castling, or if the king was in check before castling.
  ///
  /// If a PV move is set with setPV(Move pv), this move will be returned first
  /// and will not be returned again at its normal position in the list.
  ///
  /// Killer moves will be played as soon as possible after non-quiet moves.
  /// Since killer moves are stored for the whole ply, a killer move might not
  /// be valid for the current position. Therefore, we wait until they are
  /// generated normally, then push them to the top of the quiet moves.
  ///
  /// @param p        Position to generate moves for
  /// @param genMode  Which types of moves to generate (captures, quiet, or both)
  /// @param evasion  If true, only generate moves that might escape check.
  ///                 This reduces the total moves generated but may still include
  ///                 some non-legal moves when calculating all scenarios would
  ///                 be more expensive than generating and dismissing later.
  ///                 Due to beta cutoffs, we often never check full legality anyway.
  /// @return         Pointer to an internal move list (do not store - will be reused)
  const MoveList* generatePseudoLegalMoves(const Position& p, GenMode genMode, bool evasion = false);

  /// Generates legal moves for the next player.
  /// Uses generatePseudoLegalMoves() and filters out illegal moves.
  /// Usually only used for root move generation as this is expensive.
  /// During alpha-beta search we use pseudo-legal move generation instead.
  /// Unlike generatePseudoLegalMoves(), this determines check and evasion internally.
  /// @param p        Position to generate moves for
  /// @param genMode  Which types of moves to generate
  /// @return         Pointer to an internal move list (do not store - will be reused)
  const MoveList* generateLegalMoves(const Position& p, GenMode genMode);

  /// Returns the next pseudo-legal move using phased (on-demand) generation.
  /// This is the main function for phased move generation during search.
  /// It returns moves one at a time and is typically called in a loop.
  /// Since we hope for an early beta cutoff, this saves time by not
  /// generating all moves upfront.
  ///
  /// To reuse this on the same position, call resetOnDemand() first.
  /// This is not necessary when called with a different position, as the
  /// function will reset itself automatically in that case.
  ///
  /// If a PV move is set with setPV(Move pv), it will be returned first
  /// and will not be returned again at its normal position.
  ///
  /// Killer moves will be played as soon as possible. Since killer moves are
  /// stored for the whole ply, a killer move might not be valid for the current
  /// position. Therefore, we wait until they are generated by the phased move
  /// generation, then push them to the top of that generation stage.
  ///
  /// @param p        Position to generate moves for
  /// @param genMode  Which types of moves to generate
  /// @param evasion  If true, only generate moves that might escape check.
  ///                 This reduces the total moves generated but may still include
  ///                 some non-legal moves when calculating all scenarios would
  ///                 be more expensive than generating and dismissing later.
  ///                 Due to beta cutoffs, we often never check full legality anyway.
  /// @return         Next move, or MOVE_NONE when all moves have been returned
  Move getNextPseudoLegalMove(const Position& p, GenMode genMode, bool evasion = false);

  /// Resets the move generator to start fresh.
  /// Clears all internal lists (including killers) and resets the on-demand iterator.
  void reset() {
    pseudoLegalMoves.clear();
    legalMoves.clear();
    killerMoves[0] = MOVE_NONE;
    killerMoves[1] = MOVE_NONE;
    resetOnDemand();
  }

  /// Resets the on-demand move generator to start fresh.
  /// Also clears the PV move. Call this to restart phased generation
  /// on the same position without clearing killer moves.
  void resetOnDemand() {
    onDemandMoves.clear();
    onDemandEvasionTargets = BbZero;
    currentODStage         = OD_NEW;
    currentODZobrist       = 0;
    pvMove                 = MOVE_NONE;
    pvMovePush             = false;
    takeIndex              = 0;
  }

  /// Sets a PV move, which should be returned first by the on-demand generator.
  /// @param move  The PV move to prioritize
  void setPV(Move move);

  /// Stores a killer move to be returned as soon as possible during on-demand generation.
  /// Killer moves are quiet moves that caused a beta cutoff at the same ply.
  /// @param killerMove  The killer move to store
  void storeKiller(Move killerMove);

  /// Provides a pointer to the search's history data for move ordering.
  /// History data is used to improve quiet move sorting based on past success.
  /// @param pHistory  Pointer to History struct (not owned)
  void setHistoryData(History* pHistory);

  /// Determines if the position has at least one legal move.
  /// We only need to find one legal move, so we search for any
  /// KING, PAWN, KNIGHT, BISHOP, ROOK, QUEEN move and return immediately
  /// when one is found. The search order is approximately from most likely
  /// to least likely piece to have a legal move.
  /// @param position  Position to check
  /// @return          True if at least one legal move exists
  static bool hasLegalMove(const Position& position);

  /// Determines if the position has at least one legal en passant capture.
  /// This is a fast check that verifies the capturing pawn is not pinned:
  /// - Horizontal pin: both pawns removed reveal rook/queen attack on king
  /// - Vertical pin: capturing pawn pinned on file by rook/queen
  /// - Diagonal pin: capturing pawn pinned by bishop/queen
  /// Used by hasLegalMove() and tablebase probing.
  /// @param position  Position to check
  /// @return          True if at least one legal en passant capture exists
  static bool hasLegalEpCapture(const Position& position);

  /// Parses a UCI move string and returns the corresponding Move.
  /// Generates all legal moves and matches the given UCI move string against them.
  /// If there is a match, the actual Move object is returned.
  /// As this uses string creation and comparison, it is not very efficient.
  /// Use only when performance is not critical.
  /// @param position  Position the move is for
  /// @param uciMove   UCI move string (e.g., "e2e4", "e7e8q")
  /// @return          Matching Move, or MOVE_NONE if no match found
  Move getMoveFromUci(const Position& position, const std::string& uciMove);

  /// Parses a SAN move string and returns the corresponding Move.
  /// Generates all legal moves and matches the given SAN move string against them.
  /// If there is a match, the actual Move object is returned.
  /// As this uses string creation and comparison, it is not very efficient.
  /// Use only when performance is not critical.
  /// @param position  Position the move is for
  /// @param sanMove   SAN move string (e.g., "e4", "Nxf3+", "O-O")
  /// @return          Matching Move, or MOVE_NONE if no match found
  Move getMoveFromSan(const Position& position, const std::string& sanMove);

  /// Validates if a move is a legal move on the given position.
  /// @param position  Position to validate against
  /// @param move      Move to validate
  /// @return          True if the move is legal
  bool validateMove(const Position& position, Move move);

  /// Returns a string representation of a MoveGenerator instance.
  /// @return Debug string
  static std::string str();

  /// Returns the currently set PV move.
  /// @return PV move, or MOVE_NONE if not set
  [[nodiscard]] Move getPvMove() const {
    return pvMove;
  }

  /// Returns a reference to the current killer move array.
  /// @return Reference to array of 2 killer moves
  [[nodiscard]] std::array<Move, 2>& getKillerMoves() {
    return killerMoves;
  }

private:
  /// Fills on-demand move list by generating moves according to the current phase.
  /// @param position  Position to generate moves for
  /// @param genMode   Which types of moves to generate
  /// @param evasion   Whether to generate only evasion moves
  void fillOnDemandMoveList(const Position& position, GenMode genMode, bool evasion);

  /// Updates move sort values based on history heuristic data.
  /// Moves that have been successful in the past get higher sort values.
  /// @param p         Position (for context)
  /// @param moveList  List of moves to update sort values for
  void updateSortValues(const Position& p, MoveList* moveList) const;

  /// Returns a bitboard of target squares for evasion moves when in check.
  /// These target squares cover the attacking (checker) piece and any squares
  /// in between the attacker and the king (for sliding attackers).
  /// Most moves will not even be generated if they don't target these squares.
  /// If there are two or more attackers, only king moves are possible.
  /// @param p  Position with check
  /// @return   Bitboard of valid target squares for evasion
  static Bitboard getEvasionTargets(const Position& p);

  /// Generates pseudo-legal pawn moves for the next player.
  /// Does not check if king is left in check.
  /// @param position       Position to generate moves for
  /// @param pMoves         Move list to append generated moves to
  /// @param genMode        Which types of moves to generate
  /// @param evasion        Whether in evasion mode
  /// @param evasionTargets Target squares for evasion (if in check)
  static void generatePawnMoves(const Position& position, MoveList* pMoves, GenMode genMode, bool evasion, Bitboard evasionTargets);

  /// Generates pseudo-legal knight, bishop, rook, and queen moves for the next player.
  /// Does not check if king is left in check.
  /// @param position       Position to generate moves for
  /// @param pMoves         Move list to append generated moves to
  /// @param genMode        Which types of moves to generate
  /// @param evasion        Whether in evasion mode
  /// @param evasionTargets Target squares for evasion (if in check)
  static void generateMoves(const Position& position, MoveList* pMoves, GenMode genMode, bool evasion, Bitboard evasionTargets);

  /// Generates pseudo-legal king moves for the next player.
  /// Does not check if king lands on an attacked square.
  /// @param position  Position to generate moves for
  /// @param pMoves    Move list to append generated moves to
  /// @param genMode   Which types of moves to generate
  /// @param evasion   Whether in evasion mode
  static void generateKingMoves(const Position& position, MoveList* pMoves, GenMode genMode, bool evasion);

  /// Generates pseudo-legal castling moves for the next player.
  /// Does not check if king passes or lands on an attacked square.
  /// @param position  Position to generate moves for
  /// @param pMoves    Move list to append generated moves to
  /// @param genMode   Which types of moves to generate
  static void generateCastling(const Position& position, MoveList* pMoves, GenMode genMode);

  FRIEND_TEST(MoveGenTest, pawnMoves);
  FRIEND_TEST(MoveGenTest, kingMoves);
  FRIEND_TEST(MoveGenTest, normalMoves);
  FRIEND_TEST(MoveGenTest, castlingMoves);
  FRIEND_TEST(MoveGenTest, storeKiller);
  FRIEND_TEST(MoveGenTest, sortValueTest);
};

#endif//FRANKYCPP_MOVEGENERATOR_H
