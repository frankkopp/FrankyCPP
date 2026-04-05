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
            ┌─────────────────┴─────────────────┐
            │         Lazy SMP Threads          │
            │                                   │
            │  T0 (main search thread)          │
            │  ├─ iterative deepening           │
            │  ├─ aspiration windows            │
            │  ├─ time management               │
            │  └─ UCI output & bestmove         │
            │                                   │
            │  T1..Tn (helper threads)          │
            │  ├─ alpha-beta only               │
            │  ├─ no aspiration/UCI output      │
            │  └─ root-move diversification     │
            └─────────────────┬─────────────────┘
                              │
        ┌─────────────┬───────┴───────┬─────────────┐
        │             │               │             │
        ▼             ▼               ▼             ▼
┌───────────────┐ ┌───────────┐ ┌───────────┐ ┌───────────────┐
│  Evaluator    │ │    TT     │ │ Tablebase │ │  OpeningBook  │
│  (scoring)    │ │(hash tbl) │ │ (Syzygy)  │ │   (library)   │
│               │ │ [shared]  │ │           │ │               │
│  ┌─────────┐  │ └───────────┘ └───────────┘ └───────────────┘
│  │ PawnTT  │  │
│  │ (pawn   │  │
│  │  hash)  │  │
│  └─────────┘  │
└───────────────┘
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
│   ├── ExePath.h/.cpp    # Executable path resolution (platform-specific)
│   ├── stringutil.h      # String manipulation and parsing helpers
│   ├── misc.h            # Miscellaneous utilities
│   └── pgn/              # PGN parser library (extracted from OpeningBook)
│       ├── PgnParser.h/.cpp  # Streaming + batch PGN parsing
│       ├── PgnGame.h         # Structured game with headers, moves, result
│       └── PgnTypes.h        # GameResult enum, conversion functions
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

tuning/ (non-production builds only)
├── extractor/            # Position extraction from PGN games
│   ├── PositionExtractor.h/.cpp  # 6 configurable filters
│   └── ExtractorMain.cpp         # CLI tool
└── optimizer/            # Texel tuning optimizer
    ├── TexelTuner.h/.cpp         # Sigmoid MSE, K-tuning, coordinate descent
    ├── TuningDataset.h/.cpp      # FEN+result dataset loader
    ├── TuningParameter.h/.cpp    # Registry → flat param vector mapping
    ├── TuningState.h/.cpp        # YAML checkpoint save/load
    ├── TuningOutput.h/.cpp       # Tuned params YAML + comparison report
    └── TunerMain.cpp             # CLI tool
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
- **MultiPV analysis mode** (top N moves with sorted, batched UCI output)
- **Lazy SMP multi-threaded parallel search**

**Tablebase Integration:**
- **Root probing:** Before search, probe tablebases to filter moves to only WDL-optimal moves
- **In-search probing:** WDL probes at interior nodes when piece count is within TB range
- **DTZ scoring:** Shorter wins score higher, longer losses score lower
- Configurable via `SyzygyPath`, `SyzygyProbeDepth`, `USE_TB_PROBE_ROOT` options

**Threading Model (Lazy SMP):**
- **Main search thread (T0):** Runs the full search with all features — iterative deepening, aspiration windows, time management, UCI `info` output, and best move reporting. Only T0 sends output to the UCI handler.
- **Helper threads (T1..Tn):** Run the same full `iterativeDeepening()` code as T0 — aspiration windows, LMR, move ordering, etc. — but with `isMainThread()` guards suppressing UCI output and time management. Each helper starts at a different depth offset (1 + id % 3) for search diversification.
- The only shared state is the **transposition table (TT)** — threads communicate implicitly by reading/writing TT entries.
- A shared `std::atomic_bool` stop flag coordinates shutdown; when T0 decides to stop (time limit, depth limit, or `stop` command), all threads exit.
- After all threads stop, **best-thread selection** compares depth and score across all threads to pick the best result (not necessarily T0's).
- Node counts are aggregated from all threads for UCI `info nodes` output.
- See `docs/Lazy_SMP_Explained.md` for a full description of the algorithm.

**MultiPV Analysis Mode:**
- Controlled by UCI option `MultiPV` (default=1, max=128)
- When MultiPV > 1, the main thread wraps each iteration's root search in a MultiPV loop: pvIdx=0 uses aspiration search, pvIdx=1..N-1 use full-window `rootSearch()` starting from index `pvIdx`
- Results are collected during the loop, then **sorted by score (descending)** and **reported as a batch** with consistent node counts (Stockfish-style)
- `rootMoves[0..N]` are re-sorted to match, ensuring post-iteration code sees the true best move
- Helper threads always use MultiPV=1 for search efficiency
- Aspiration `lowerbound`/`upperbound` UCI output is suppressed when MultiPV > 1 to prevent GUI display artifacts

**Owned Components:**
- `TT` - Transposition table (shared across all SMP threads)
- `Evaluator` - Position evaluation (each thread has its own, with per-thread `PawnTT`)
- `OpeningBook` - Opening library (optional)
- `Tablebase` - Syzygy endgame tablebases (optional)
- `History` - History heuristic data (per-thread)
- `plyStack` - Per-ply search state array (per-thread)

**Per-Ply State (`PlyInfo`):**

Each ply level has its own `PlyInfo` struct containing:
- `MoveGenerator` instances (normal + singular verification)
- Current/excluded move tracking
- Static evaluation cache
- Move count and check status

```cpp
class Search {
  std::unique_ptr<TT> tt;           // Shared across all SMP threads
  std::unique_ptr<OpeningBook> book;
  std::unique_ptr<Tablebase> tablebase;

  // Per-thread state (index 0 = main thread, 1..N-1 = helpers)
  // Each SearchThreadData owns: Position, Evaluator, PVTable, History, PlyInfo stack, rootMoves,
  //   completedIterationDepth, lastIterationValue (for best-thread selection)
  std::vector<std::unique_ptr<SearchThreadData>> searchThreadData;

  std::thread searchThread;                    // Main search thread
  std::vector<std::thread> helperThreads;      // Lazy SMP helper threads
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
  Spawn Lazy SMP helper threads (T1..Tn)
  Each thread gets own Position copy, History, PlyStack
         │
  ┌──────┴──────────────────────────────────┐
  │ T0 (main)      T1 (helper) ... Tn (helper) │
  │                                           │
  │  Search::iterativeDeepening()  (all threads, independent)
  │         │
  │    ┌────┴────┐
  │    │ depth=1 │  (helpers may start at depth=2, diversified)
  │    │ depth=2 │
  │    │   ...   │
  │    │ depth=N │
  │    └────┬────┘
  └──────────────────────────────────────────┘
         │  (all threads share TT read/write)
         ▼
Search::pvSearch(alpha, beta, depth)  ◄──┐
         │                               │
         ├──► TT probe  (shared TT)      │
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
TT store result  (shared TT — all threads contribute)
         │
  stopFlag set → helper threads exit, nodes aggregated
         │
         ▼
  selectBestThread() → compare depth+score across all threads
         │
         ├──► TB root override (if applicable)
         ├──► Ponder move extraction (PV or TT fallback)
         │
         ▼
UciHandler::sendFinalUciInfo() + sendResult()
```

---

## Threading Model

FrankyCPP uses **Lazy SMP** for parallel search — multiple threads run independent alpha-beta searches sharing only the transposition table.

| Thread                  | Purpose                                                                                                   |
|-------------------------|-----------------------------------------------------------------------------------------------------------|
| Main thread             | UCI command loop (`UciHandler::loop()`)                                                                   |
| Search thread (T0)      | **Main search:** iterative deepening, aspiration windows, time management, UCI output, best move decision |
| Helper threads (T1..Tn) | **Full search:** same iterative deepening as T0, with depth offset diversification, no UCI output         |
| Timer thread            | Monitors time limits, sets stop flag                                                                      |

**Main Search Thread (T0) Responsibilities:**
- Runs full iterative deepening with aspiration windows
- Manages time control and decides when to stop
- Sends all UCI `info` strings (depth, score, pv, nodes, nps, etc.)
- Reports the final `bestmove` to the GUI
- Coordinates helper thread lifecycle (spawn after TT priming, join on stop)

**Helper Threads (T1..Tn) Responsibilities:**
- Run full `iterativeDeepening()` — same code as T0 (aspiration windows, LMR, etc.)
- **No UCI output** — all `send*()` calls guarded by `isMainThread()`
- **No time management** — only check `stopSearchFlag`, no time decisions
- **Depth offset diversification** — start at depth (1 + id % 3) to spread search across different depths
- Contribute to TT population — their search results help all threads find better moves
- Track `completedIterationDepth` and `lastIterationValue` for best-thread selection
- Exit immediately when stop flag is set

**Best Thread Selection:**
- After all helpers join, `selectBestThread()` compares all threads using a depth+score heuristic
- Deeper thread wins unless its score is worse by more than `BEST_THREAD_SCORE_MARGIN` (default: 50cp)
- If a helper thread is selected, a final UCI `info` line is sent before `bestmove` for GUI consistency
- TB root override and ponder move extraction apply on top of the selected thread's result
- Configurable via UCI options `Best Thread Selection` and `Best Thread Score Margin`

**Lazy SMP Design:**
- Each thread holds its own `Position` copy, `History` tables, and `PlyInfo` stack — **no locking on hot paths**
- The `TT` (transposition table) is the **only shared data structure**; threads communicate implicitly through it
- A single `std::atomic_bool` stop flag stops all threads simultaneously
- The best move comes from the best thread's search (depth+score selection across all threads)

**Synchronization:**
- `std::binary_semaphore` for search init/running state handoff
- `std::atomic_bool` for stop flag (all threads poll this)
- `std::atomic<uint64_t>` for aggregated node count
- TT uses lock-free access (memory fences / accepted benign races on entry writes)

**Thread count** is configurable via the UCI `Threads` option (default: 1, i.e. single-threaded).  
See `docs/Lazy_SMP_Explained.md` for a detailed explanation of the algorithm and its performance characteristics.

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

| Target                     | Description                              |
|----------------------------|------------------------------------------|
| `FrankyCPP_v1.8`           | Main UCI engine executable               |
| `FrankyCPP_v1.8_Test`      | GoogleTest unit tests                    |
| `FrankyCPP_v1.8_Bench`     | Google Benchmark microbenchmarks         |
| `FrankyCPP_v1.8_Extractor` | Position extractor (non-production only) |
| `FrankyCPP_v1.8_Tuner`     | Texel tuning optimizer (non-production)  |

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

*Last updated: 2026-03-31*
