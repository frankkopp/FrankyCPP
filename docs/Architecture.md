# FrankyCPP Architecture

This document describes the high-level architecture of FrankyCPP, a UCI chess engine written in modern C++20.

---

## Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                              main.cpp                                │
│                    (CLI parsing, initialization)                     │
└─────────────────────────────────┬───────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                            UciHandler                                │
│              (UCI protocol, command loop, I/O streams)               │
└───────┬─────────────────────┬─────────────────────┬─────────────────┘
        │                     │                     │
        ▼                     ▼                     ▼
┌───────────────┐     ┌───────────────┐     ┌───────────────┐
│   Position    │     │    Search     │     │     Perft     │
│ (board state) │     │ (alpha-beta)  │     │  (debugging)  │
└───────────────┘     └───────┬───────┘     └───────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌───────────────┐     ┌───────────────┐     ┌───────────────┐
│  Evaluator    │     │      TT       │     │  OpeningBook  │
│  (scoring)    │     │ (hash table)  │     │   (library)   │
└───────────────┘     └───────────────┘     └───────────────┘
```

---

## Directory Structure

```
src/
├── main.cpp              # Entry point, CLI parsing (Boost.ProgramOptions)
├── init.h                # Global initialization dispatcher
│
├── types/                # Core value types (zero-cost abstractions)
│   ├── bitboard.h        # Bitboard wrapper with chess operations
│   ├── move.h            # 32-bit encoded move with sort value
│   ├── square.h          # Square type (0-63 mapping)
│   ├── piece.h           # Piece and PieceType enums
│   ├── value.h           # Evaluation value type
│   ├── types.h           # Aggregated type includes
│   └── ...               # Additional type headers
│
├── common/               # Shared utilities
│   ├── Logging.h         # spdlog-based logging with compile-time levels
│   ├── ThreadPool.h      # Generic thread pool for parallel tasks
│   ├── stringutil.h      # String manipulation helpers
│   └── misc.h            # Miscellaneous utilities
│
├── chesscore/            # Chess logic (board, moves, rules)
│   ├── Position.h/.cpp   # Board state, make/unmake, Zobrist keys
│   ├── MoveGenerator.h   # Legal/pseudo-legal move generation
│   ├── Zobrist.h         # Zobrist key generation tables
│   ├── History.h         # History heuristic data structures
│   ├── Perft.h           # Perft testing and validation
│   └── Values.h          # Piece values, PST tables
│
├── engine/               # Search and evaluation
│   ├── Search.h/.cpp     # Alpha-beta with iterative deepening
│   ├── Evaluator.h/.cpp  # Position evaluation
│   ├── TT.h/.cpp         # Transposition table
│   ├── PawnTT.h          # Dedicated pawn hash table
│   ├── See.h             # Static exchange evaluation
│   ├── UciHandler.h/.cpp # UCI protocol implementation
│   ├── SearchLimits.h    # Time/depth/node limits
│   ├── SearchResult.h    # Search result container
│   ├── SearchStats.h     # Statistics collection
│   ├── UciOptions.h      # UCI option definitions
│   └── config/           # YAML configuration system
│       ├── ConfigManager.h    # Singleton config access
│       ├── SearchConfigData.h # Search parameters (~60 options)
│       ├── EvalConfigData.h   # Evaluation parameters (~40 options)
│       └── ...
│
├── openingbook/          # Opening book support
│   ├── OpeningBook.h/.cpp # Book loading, querying, caching
│   └── bookentry.h       # Book entry data structure
│
└── enginetest/           # Built-in test suite runner
    ├── TestSuite.h       # EPD test suite execution
    └── SearchTreeSizeTest.h # Search tree analysis
```

---

## Key Components

### UciHandler

The main entry point after initialization. Implements the [UCI protocol](http://wbec-ridderkerk.nl/html/UCIProtocol.html).

**Responsibilities:**
- Parse UCI commands from input stream
- Manage `Position`, `Search`, `Perft` instances
- Send search results and info strings to output stream
- Handle `go`, `position`, `ucinewgame`, `setoption`, etc.

**Key Design:**
- Accepts custom `istream`/`ostream` for testability
- Owns the `Search` instance via `shared_ptr`
- Creates new `Position` for each `position` command

```cpp
class UciHandler {
  std::unique_ptr<Position> pPosition;
  std::shared_ptr<Search> pSearch;
  std::istream* pInputStream;
  std::ostream* pOutputStream;
  // ...
};
```

---

### Position

Represents the complete state of a chess position.

**State Maintained:**
- 8×8 piece array + bitboards per piece type/color (hybrid representation)
- Zobrist key (incrementally updated)
- Pawn-specific Zobrist key (for pawn hash table)
- Castling rights, en passant square, half-move clock
- History stack for undo support
- Material and positional value counters

**Key Operations:**
- `doMove(Move)` / `undoMove()` - make/unmake with full state restoration
- `isLegalMove(Move)` - legality check
- `isAttacked(Square, Color)` - attack detection
- `getZobristKey()` - hash key for TT lookup

```cpp
class Position {
  ZobristKey zobristKey;
  std::array<Piece, 64> board;
  std::array<Bitboard, 14> piecesBb;  // per piece type
  std::vector<HistoryState> historyStack;
  // ...
};
```

---

### MoveGenerator

Generates legal and pseudo-legal moves for a position.

**Generation Modes:**
- **Full generation** - All moves at once (for perft, root)
- **On-demand generation** - Phased generation for search efficiency

**Move Ordering Stages (on-demand):**
1. PV move (from previous iteration)
2. TT move (hash move)
3. Captures (MVV-LVA or SEE ordered)
4. Killer moves (2 per ply)
5. Counter move
6. Quiet moves (history-sorted)

```cpp
class MoveGenerator {
  MoveList pseudoLegalMoves;
  MoveList legalMoves;
  MoveList onDemandMoves;
  // ...
  Move* getNextMove(Position&, GenMode, Move pvMove, Move ttMove, ...);
};
```

---

### Search

The core search algorithm using alpha-beta with iterative deepening.

**Features:**
- Principal Variation Search (PVS)
- Aspiration windows
- Null-move pruning (with verification)
- Late Move Reductions (LMR)
- Futility pruning
- Razoring
- Quiescence search with SEE pruning
- Time management with complexity-based allocation

**Threading Model:**
- Search runs in a dedicated thread (`searchThread`)
- Can be stopped asynchronously via `stopSearchFlag`
- Timer thread monitors time limits
- Semaphores manage initialization and running state

**Owned Components:**
- `TT` - Transposition table
- `Evaluator` - Position evaluation
- `OpeningBook` - Opening library (optional)
- `History` - History heuristic data

```cpp
class Search {
  std::unique_ptr<TT> tt;
  std::unique_ptr<Evaluator> evaluator;
  std::unique_ptr<OpeningBook> book;
  History history;
  
  std::thread searchThread;
  std::atomic_bool stopSearchFlag;
  // ...
};
```

---

### Evaluator

Calculates a heuristic value for a position.

**Evaluation Components:**
- Material balance
- Piece-square tables (midgame + endgame)
- Pawn structure (isolated, doubled, passed, connected)
- Mobility (per piece type)
- King safety
- Game phase interpolation (tapered eval)

**Caching:**
- Pawn hash table (`PawnTT`) for pawn structure evaluation

```cpp
class Evaluator {
  PawnTT pawnCache;
  const EvalConfigData& EvalConfig;  // 40+ tunable parameters
  
  Value evaluate(const Position& p);
};
```

---

### TT (Transposition Table)

Hash table storing search results for positions.

**Entry Structure (16 bytes):**
- Zobrist key (for verification)
- Best move
- Evaluation value
- Depth
- Value type (exact, lower bound, upper bound)
- Age (for replacement)

**Features:**
- Power-of-two sizing for fast indexing
- Prefetch support for CPU cache optimization
- Configurable size via UCI option

```cpp
class TT {
  std::unique_ptr<TTEntry[]> entries;
  uint64_t numEntries;
  uint8_t currentAge;
  // ...
};
```

---

### ConfigManager

Singleton providing runtime-configurable parameters.

**Design:**
- Lazy auto-initialization on first access
- Loads from `config/search.yaml` and `config/eval.yaml`
- Falls back to compiled defaults if files missing
- Override support for testing (`CONFIG_OVERRIDE_START/END` macros)

**Usage:**
```cpp
// Access search parameters
const auto& cfg = ConfigManager::instance().search();
if (cfg.USE_NULL_MOVE) { ... }

// Override in tests
CONFIG_OVERRIDE_START()
  s.USE_PVS = false;
  e.USE_MOBILITY = false;
CONFIG_OVERRIDE_END();
```

---

## Data Flow

### Search Flow

```
UciHandler receives "go" command
         │
         ▼
Search::startSearch(Position, SearchLimits)
         │
         ├──► Check opening book for book move
         │
         ▼
Search::iterativeDeepening()
         │
    ┌────┴────┐
    │ depth=1 │
    │ depth=2 │
    │   ...   │
    │ depth=N │
    └────┬────┘
         │
         ▼
Search::pvSearch(alpha, beta, depth)  ◄──┐
         │                               │
         ├──► TT probe                   │
         ├──► Null-move pruning          │
         ├──► MoveGenerator (on-demand)  │
         │         │                     │
         │         ▼                     │
         │    For each move:             │
         │    ├──► Position::doMove()    │
         │    ├──► pvSearch() (recursive)┘
         │    ├──► Position::undoMove()
         │    └──► Update best move
         │
         ├──► Quiescence search (at depth 0)
         │
         ▼
TT store result
         │
         ▼
UciHandler::sendSearchUpdate() / sendResult()
```

---

## Threading Model

FrankyCPP currently uses **single-threaded search** with auxiliary threads for:

| Thread | Purpose |
|--------|---------|
| Main thread | UCI command loop (`UciHandler::loop()`) |
| Search thread | Executes search algorithm |
| Timer thread | Monitors time limits, triggers stop |

**Synchronization:**
- `std::binary_semaphore` for init/running state
- `std::atomic_bool` for stop flag
- No locking in TT (single-threaded search)

**Future:** Lazy SMP multi-threaded search is planned (see roadmap).

---

## Configuration System

```
config/
├── search.yaml    # Search parameters (60+ options)
├── eval.yaml      # Evaluation parameters (40+ options)
└── FrankyCPP.cfg  # Legacy config (Boost.ProgramOptions)
```

**YAML Structure (example):**
```yaml
# search.yaml
USE_PVS: true
USE_NULL_MOVE: true
NULL_MOVE_REDUCTION: 3
LMR_FULL_DEPTH_MOVES: 4
...
```

**Access Pattern:**
```cpp
ConfigManager::instance().search().USE_PVS
ConfigManager::instance().eval().USE_MOBILITY
```

---

## Build Targets

| Target | Description |
|--------|-------------|
| `FrankyCPP_v0.7` | Main UCI engine executable |
| `FrankyCPP_v0.7_Test` | GoogleTest unit tests |
| `FrankyCPP_v0.7_Bench` | Google Benchmark microbenchmarks |

---

## Dependencies

| Library | Purpose | Integration |
|---------|---------|-------------|
| Boost.ProgramOptions | CLI argument parsing | vcpkg |
| Boost.Serialization | Opening book caching | vcpkg |
| spdlog | Logging (header-only) | vcpkg |
| yaml-cpp | YAML config parsing | vcpkg |
| GoogleTest | Unit testing | vcpkg |
| Google Benchmark | Performance testing | vcpkg |

---

*Last updated: 2026-01-30*
