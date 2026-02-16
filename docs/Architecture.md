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
        ┌─────────────┬───────┴───────┬─────────────┐
        │             │               │             │
        ▼             ▼               ▼             ▼
┌───────────────┐ ┌───────────┐ ┌───────────┐ ┌───────────────┐
│  Evaluator    │ │    TT     │ │ Tablebase │ │  OpeningBook  │
│  (scoring)    │ │(hash tbl) │ │ (Syzygy)  │ │   (library)   │
└───────────────┘ └───────────┘ └───────────┘ └───────────────┘
```

---

## Command-Line Interface

FrankyCPP supports various CLI options for different modes of operation:

### Information Options
```bash
FrankyCPP --help           # Show all available options
FrankyCPP --version        # Show version information
FrankyCPP --ucioptions     # Print UCI options (like 'uci' command)
```

### Configuration Discovery
```bash
FrankyCPP --show-config                    # Show all settings (table format)
FrankyCPP --show-config --format yaml      # Generate YAML template
FrankyCPP --show-config --format json      # Generate JSON for tooling
FrankyCPP --show-config --domain search    # Filter by domain
```

### Testing & Benchmarking
```bash
FrankyCPP --perft --startDepth 1 --endDepth 6    # Run perft test
FrankyCPP --bench --benchDepth 12 --benchHash 256  # Run benchmark
FrankyCPP --testsuite file.epd --tsTime 1000     # Run test suite
```

### Configuration
```bash
FrankyCPP --config path/to/config.cfg      # Use custom config file
FrankyCPP --nobook                         # Disable opening book
FrankyCPP --book path/to/book.txt --booktype simple  # Custom book
```

### Syzygy Tablebases
```bash
FrankyCPP --syzygy status                  # Show tablebase status
FrankyCPP --syzygy download --pieces 3-4-5 --path D:\Chess\Syzygy  # Download TBs
FrankyCPP --syzygy verify --path D:\Chess\Syzygy   # Verify TB files
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
│   ├── stringutil.h      # String manipulation and parsing helpers
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
├── config/               # Configuration system (single source of truth)
│   ├── ConfigRegistry.h/.cpp  # Central registry of all config definitions
│   ├── ConfigManager.h/.cpp   # Singleton config access, YAML loading
│   ├── ConfigGenerators.h/.cpp # Auto-generated str(), YAML, UCI from registry
│   ├── ConfigDef.h       # ConfigDef struct and value type helpers
│   ├── SearchConfigData.h # Search parameters struct (~70 options)
│   └── EvalConfigData.h  # Evaluation parameters struct (~50 options)
│
├── engine/               # Search and evaluation
│   ├── Search.h/.cpp     # Alpha-beta with iterative deepening
│   ├── PlyInfo.h         # Per-ply search state (MoveGenerators, moves, eval)
│   ├── PVTable.h         # Triangular PV table for efficient PV storage
│   ├── Evaluator.h/.cpp  # Position evaluation
│   ├── TT.h/.cpp         # Transposition table
│   ├── PawnTT.h          # Dedicated pawn hash table
│   ├── See.h             # Static exchange evaluation
│   ├── UciHandler.h/.cpp # UCI protocol implementation
│   ├── UciOptions.h/.cpp # UCI option handling (uses ConfigRegistry)
│   ├── SearchLimits.h    # Time/depth/node limits
│   ├── SearchResult.h    # Search result container
│   └── SearchStats.h     # Statistics collection
│
├── openingbook/          # Opening book support
│   ├── OpeningBook.h/.cpp # Book loading, querying, caching
│   └── bookentry.h       # Book entry data structure
│
├── tablebase/            # Syzygy endgame tablebase support
│   ├── Tablebase.h/.cpp  # Fathom library interface (WDL/DTZ probing)
│   ├── TablebasePaths.h/.cpp # TB path discovery and validation
│   └── TablebaseDownloader.h/.cpp # TB download from Lichess mirror
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
- Time management with complexity-based allocation and best-move instability detection
- Syzygy tablebase probing for endgame positions

**Tablebase Integration:**
- **Root probing:** Before search, probe tablebases to filter moves to only WDL-optimal moves
- **In-search probing:** WDL probes at interior nodes when piece count is within TB range
- **DTZ scoring:** Shorter wins score higher, longer losses score lower
- Configurable via `SyzygyPath`, `SyzygyProbeDepth`, `USE_TB_PROBE_ROOT` options

**Threading Model:**
- Search runs in a dedicated thread (`searchThread`)
- Can be stopped asynchronously via `stopSearchFlag`
- Timer thread monitors time limits
- Semaphores manage initialization and running state

**Owned Components:**
- `TT` - Transposition table
- `Evaluator` - Position evaluation
- `OpeningBook` - Opening library (optional)
- `Tablebase` - Syzygy endgame tablebases (optional)
- `History` - History heuristic data
- `plyStack` - Per-ply search state array

**Per-Ply State (`PlyInfo`):**

Each ply level has its own `PlyInfo` struct containing:
- `MoveGenerator` instances (normal + singular verification)
- Current/excluded move tracking
- Static evaluation cache
- Move count and check status

```cpp
class Search {
  std::unique_ptr<TT> tt;
  std::unique_ptr<Evaluator> evaluator;
  std::unique_ptr<OpeningBook> book;
  std::unique_ptr<Tablebase> tablebase;
  History history;
  
  // Per-ply search state - each PlyInfo owns its MoveGenerators
  std::array<PlyInfo, DEPTH_MAX + 1> plyStack;
  
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

### Tablebase (Syzygy)

Interface to Syzygy endgame tablebases via the Fathom library.

**Tablebase Types:**
- **WDL (Win/Draw/Loss):** Fast probe for search decisions
- **DTZ (Distance To Zeroing):** For optimal root move selection, accounts for 50-move rule

**WDL Results:**

| Result      | Meaning                                        |
|-------------|------------------------------------------------|
| Win         | Position is won with perfect play              |
| CursedWin   | Would be won but may draw due to 50-move rule  |
| Draw        | Position is drawn with perfect play            |
| BlessedLoss | Would be lost but may draw due to 50-move rule |
| Loss        | Position is lost with perfect play             |

**Limitations:**
- Only positions without castling rights
- Piece count must be within available TB range (typically 6-7 pieces)
- Requires tablebase files on disk (~150GB for 7-piece)

**Key Operations:**
- `probeWDL(pos)` - Fast WDL probe for search nodes (thread-safe)
- `probeRoot(pos)` - Full probe with DTZ and best move (root only)
- `canProbe(pos)` - Check if position is probeable

**UCI Options:**
- `SyzygyPath` - Path(s) to tablebase files (semicolon-separated on Windows)
- `SyzygyProbeDepth` - Minimum depth for in-search probing
- `SyzygyProbeRoot` - Enable root probing (move filtering)

```cpp
class Tablebase {
  bool initialized_;
  int maxPieces_;      // Max pieces in loaded TBs (e.g., 6 or 7)
  std::string tbPath_;
  
  TBResult probeWDL(const Position& pos) const;
  TBProbeResult probeRoot(const Position& pos) const;
  bool canProbe(const Position& pos) const;
};
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
         ├──► Probe tablebases at root (filter moves to TB-optimal)
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
         ├──► Tablebase WDL probe        │
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

| Thread        | Purpose                                 |
|---------------|-----------------------------------------|
| Main thread   | UCI command loop (`UciHandler::loop()`) |
| Search thread | Executes search algorithm               |
| Timer thread  | Monitors time limits, triggers stop     |

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

| Target                 | Description                      |
|------------------------|----------------------------------|
| `FrankyCPP_v0.7`       | Main UCI engine executable       |
| `FrankyCPP_v0.7_Test`  | GoogleTest unit tests            |
| `FrankyCPP_v0.7_Bench` | Google Benchmark microbenchmarks |

---

## Dependencies

| Library              | Purpose                  | Integration        |
|----------------------|--------------------------|--------------------|
| Boost.ProgramOptions | CLI argument parsing     | vcpkg              |
| Boost.Serialization  | Opening book caching     | vcpkg              |
| spdlog               | Logging (header-only)    | vcpkg              |
| yaml-cpp             | YAML config parsing      | vcpkg              |
| GoogleTest           | Unit testing             | vcpkg              |
| Google Benchmark     | Performance testing      | vcpkg              |
| Fathom               | Syzygy tablebase probing | CMake FetchContent |

---

*Last updated: 2026-02-16*
