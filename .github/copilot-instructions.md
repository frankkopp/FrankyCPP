# FrankyCPP Copilot Instructions

---

## ⚠️ Code Change Authorization Rules

**CRITICAL: Only make code changes of other write operations (e.g. git commit) when explicitly requested or clearly implied by user intent**

### When CODE CHANGES are allowed:
- User uses action verbs: "implement", "fix", "change", "update", "add", "remove", "refactor", "proceed"
- User gives explicit permission: "go ahead", "do it", "make the changes"
- User provides specific requirement: "I want X to do Y", "Change X to Y"
- Follow-up after analysis: User reviews findings and says "fix it", "apply the fix"

### When CODE CHANGES are FORBIDDEN:
- User requests information: "check", "show", "analyze", "review", "what", "how", "why", "explain", "wdyt"
- User asks for opinion: "should I", "what do you think", "is this correct", "any issues"
- User requests analysis: "look at the logs", "check if", "verify", "investigate"
- User says "no code change" explicitly
- **Exception:** Read-only operations are ALWAYS allowed (read files, grep, check logs, query API)

### Gray areas - ASK FIRST:
- User reports a problem but doesn't explicitly say "fix it"
- User shows logs/errors without requesting action
- Ambiguous requests like "thoughts?" or "what about this?"

### Non-code actions ALWAYS allowed:
- Reading files
- Searching text
- Analyzing logs
- Providing explanations or suggestions without code changes
- Asking clarifying questions
- Providing code snippets without applying changes
- Guiding user through manual steps

### Non-code actions FORBIDDEN:
- Making code changes without explicit permission
- Committing code without explicit permission
- Pushing code to remote repositories without explicit permission
- Any write operations not explicitly authorized by the user, e.g. modifying files, changing configurations, etc.

### Documentation Policy:
**DO NOT create summary .md documents unless explicitly requested**
- Provide summaries in chat responses instead
- User will ask for documentation if needed
- Keep detailed explanations in conversation, not new files

**Exceptions - When documentation IS appropriate:**
- Required documentation (API docs, design docs explicitly requested by user)
- Planning documents (plan, specs, design discussions) - if they exist, update them with progress
- Status tracking in existing planning documents:
  - Mark completed phases/tasks with ✅
  - Update status fields (In Progress, Complete, etc.)
  - Add brief notes (1-2 lines) so another chat session can continue
  - Keep details in chat, only high-level status in document
- DO NOT create "implementation summary" or "changes summary" documents

### Build Policy:
**DO NOT attempt to compile/build code using terminal commands**
- The terminal environment may not have the proper build tools or configuration
- Always ask the user to build after making code changes
- Instead of running `cmake`, `ninja`, `make`, etc., provide instructions like:
  - "Please rebuild the project in CLion"
  - "Run: `cmake --build cmake-build-release`"
  - "Build the target using your IDE"
- Exception: You may check if files exist or list directories
- After providing a fix, say: "Please rebuild and test to verify the fix"

---

## Project Overview

FrankyCPP is a UCI chess engine written in modern C++20. It features alpha-beta search with advanced pruning, a classical evaluation function, opening book support, and comprehensive testing infrastructure.

---

## Build Environment

### Platform & Toolchain
- **Primary Platform**: Windows 11 with MSVC 2022 (Visual Studio 17.x)
- **IDE**: CLion with MSVC toolchain
- **Build System**: CMake 3.16+ with Ninja generator
- **Package Manager**: vcpkg in manifest mode (`vcpkg.json`)
- **C++ Standard**: C++20 (enforced globally)

### Key Build Commands
```powershell
# Configure (from project root)
cmake -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build cmake-build-release --config Release

# Run tests
cmake --build cmake-build-release --target FrankyCPP_v0.7_Test
.\cmake-build-release\test\FrankyCPP_v0.7_Test.exe
```

### Dependencies (via vcpkg)
- Boost (program_options, serialization)
- spdlog (header-only logging)
- yaml-cpp (configuration files)
- GoogleTest (unit testing)
- Google Benchmark (performance benchmarks)

### Build Environment Validation
- `setup_windows_build_env.ps1` - Windows build environment setup and validation script (PowerShell)
  - **DEFAULT: Validate-only** (safe, no system modifications)
  - Requires explicit `-Install` flag to set up vcpkg
  - Checks Visual Studio version (>= 2019 16.10 for C++20)
  - Checks CMake, Ninja, Git availability
  - Can automatically clone and bootstrap vcpkg to C:\vcpkg
  - Cannot install VS/CMake/Ninja (must be done manually)
  - Usage: `.\setup_windows_build_env.ps1 [-Install] [-Help]`
- `setup_linux_build_env.sh` - Unified Linux build environment setup and validation script
  - **DEFAULT: Validate-only** (safe, no system modifications)
  - Requires explicit `--install` flag to modify system
  - Can run in install or validate modes
  - Installs all build tools (gcc, cmake, ninja, pkg-config, etc.)
  - Installs optional vcpkg dependencies (autoconf, automake, libtool)
  - Clones and bootstraps vcpkg
  - Configures VCPKG_ROOT environment variable
  - Detects CI/CD mode and adjusts paths (/opt/vcpkg for CI, ~/vcpkg for local)
  - Validates compiler versions, tool availability, CPU features
  - Idempotent - safe to run multiple times
  - Usage: `./setup_linux_build_env.sh [--install|--help]`
- CMakeLists.txt includes automatic validation during configuration
  - Enforces minimum compiler versions (GCC 10+, Clang 10+, MSVC 2019 16.10+)
  - Checks for required tools on Unix (pkg-config, tar, unzip)
  - Validates vcpkg toolchain setup
  - Provides detailed status messages for debugging

---

## Code Style & Conventions

### Naming Conventions
- **Classes**: `PascalCase` (e.g., `MoveGenerator`, `SearchConfig`)
- **Functions/Methods**: `camelCase` (e.g., `generatePseudoLegalMoves`, `getZobristKey`)
- **Variables**: `camelCase` (e.g., `searchDepth`, `bestMove`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `MAX_PLY`, `VALUE_INFINITE`)
- **Namespaces**: `lowercase` (e.g., `engine::config`)
- **File names**: `PascalCase.cpp/.h` for classes, `lowercase.h` for type headers

### Header Guards
Use `#ifndef FRANKYCPP_*` pattern consistently:
```cpp
#ifndef FRANKYCPP_POSITION_H
#define FRANKYCPP_POSITION_H
// ...
#endif // FRANKYCPP_POSITION_H
```

### Code Formatting
- `.clang-format` is present in the project root - follow it
- Use `// clang-format off` only for alignment-sensitive tables (piece-square tables, etc.)
- Maximum line length: 120 characters
- Braces: Allman style for functions, attached for control structures

### Class Layout Guidelines
Classes should follow this consistent structure for readability:

```cpp
class ClassName {
  // 1. Constants (static constexpr)
  static constexpr int MAX_SIZE = 100;
  
  // 2. Member fields (private by default)
  std::string name;
  int value;
  
public:
  // 3. Public methods (constructors, destructors, core functionality)
  ClassName();
  ~ClassName();
  
  void doSomething();
  bool processThing();
  
private:
  // 4. Private methods (helpers, implementation details)
  void helperMethod();
  bool validateInternal();
  
public:
  // 5. Getters/Setters (at the end)
  [[nodiscard]] const std::string& getName() const { return name; }
  void setName(const std::string& n) { name = n; }
  [[nodiscard]] int getValue() const { return value; }
};
```

**Rationale:** This layout makes classes easy to read and understand:
- Constants are visible at the top
- Fields are grouped together
- Public interface is prominent
- Private implementation details are separated
- Getters/setters are at the end (often trivial)

### Implementation File Order
Implementation files (.cpp) should follow the same order as the header file (.h):

```cpp
// Header declares:
class Example {
  int value;
public:
  Example();           // 1. Constructor
  void methodA();      // 2. Public method A
  void methodB();      // 3. Public method B
private:
  void helperX();      // 4. Private helper X
  void helperY();      // 5. Private helper Y
public:
  int getValue() const; // 6. Getter
};

// Implementation follows same order:
Example::Example() { }           // 1. Constructor
void Example::methodA() { }      // 2. Public method A
void Example::methodB() { }      // 3. Public method B
void Example::helperX() { }      // 4. Private helper X
void Example::helperY() { }      // 5. Private helper Y
int Example::getValue() const { } // 6. Getter
```

**Benefits:**
- Easy to navigate between header and implementation
- Consistent structure across all files
- No hunting for method implementations
- Clear correspondence between declaration and definition

### Modern C++ Guidelines
- Prefer `constexpr` for compile-time constants
- Use `[[nodiscard]]` for functions where ignoring return value is likely a bug
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw pointers for ownership
- Use `std::string_view` for non-owning string parameters
- Prefer range-based for loops
- Use structured bindings where appropriate

---

## Architecture Overview

### Directory Structure
```
src/
├── types/          # Core value types (Bitboard, Move, Square, Value, etc.)
├── common/         # Utilities (Logging, ThreadPool, string utils)
├── chesscore/      # Board representation, move generation, position
├── engine/         # Search, evaluation, UCI handler, TT
├── enginetest/     # Test suite runner
└── openingbook/    # Opening book handling
```

### Key Classes
| Class           | Responsibility                               |
|-----------------|----------------------------------------------|
| `Position`      | Board state, move execution, Zobrist hashing |
| `MoveGenerator` | Legal/pseudo-legal move generation           |
| `Search`        | Alpha-beta search with iterative deepening   |
| `Evaluator`     | Position evaluation                          |
| `TT`            | Transposition table                          |
| `UciHandler`    | UCI protocol implementation                  |
| `ConfigManager` | YAML configuration management (singleton)    |

### Configuration System
- Runtime config via YAML files in `config/` directory
- `SearchConfigData` and `EvalConfigData` structs hold all parameters
- Access via `ConfigManager::instance().search()` and `.eval()`
- Override in tests using `CONFIG_OVERRIDE_START()` / `CONFIG_OVERRIDE_END()` macros

---

## Testing

### Test Framework
- GoogleTest for unit tests
- Google Benchmark for performance tests
- Tests mirror source structure in `test/` directory

### Running Tests
```powershell
# Run all tests (excluding slow ones)
.\cmake-build-release\test\FrankyCPP_v0.7_Test.exe -*SpeedTests.*:-*TimingTests.*

# Run specific test
.\cmake-build-release\test\FrankyCPP_v0.7_Test.exe --gtest_filter=PositionTest.*
```

### Writing Tests
- Use `FRIEND_TEST` macro for testing private members
- Place test files in corresponding `test/<module>/` directory
- Use `Test_Fens.h` for standard test positions

### ⚠️ CRITICAL: Test Class Initialization
**Every test class that uses MoveGenerator, Position, or any chess move functionality MUST initialize the attack tables (magic bitboards).**

Without initialization, sliding piece moves (bishops, rooks, queens) will NOT be generated correctly.

**Required pattern for ALL chess-related test classes:**
```cpp
class YourTestClass : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();  // ← CRITICAL: Initializes attack tables/magics
    NEWLINE;
  }

protected:
  void SetUp() override {
    // Your per-test setup here
  }
};
```

**Why this is required:**
- Move generation relies on pre-computed magic bitboards for sliding pieces
- `init::init()` sets up attack tables, Zobrist keys, and other static data
- Without initialization, `MoveGenerator` will not generate bishop/rook/queen moves
- This is a one-time setup per test suite (not per test)

**Symptoms of missing initialization:**
- Bishop, rook, queen moves missing from legal move lists
- Castling moves may fail (requires attack detection)
- Position evaluation may be incorrect
- Tests that should pass will fail mysteriously

**Always add this when creating new test files for:**
- `MoveGenerator` tests
- `Position` tests
- `Search` tests
- Any test using `matchesExpectedMove()` or move comparison
- Integration tests involving chess moves

---

## Chess-Specific Notes

### Move Representation
- `Move` is a 32-bit value encoding: from, to, move type, promotion piece, sort value
- Use `Move::str()` for UCI notation (e.g., "e2e4", "e7e8q")
- `MOVE_NONE` represents no move / invalid move

### Bitboard Operations
- `Bitboard` wraps `uint64_t` with chess-specific operations
- Uses PEXT (BMI2) for sliding piece attacks - requires compatible CPU
- Square mapping: a1=0, h1=7, a8=56, h8=63 (little-endian rank-file)

### Search
- Iterative deepening with aspiration windows
- PVS (Principal Variation Search) with null-move pruning, LMR
- Time management via `SearchLimits` structure
- Search can be stopped via `stopSearch()` or time limit

---

## Windows & PowerShell Notes

### General
- **You are in a Windows PowerShell terminal - use PowerShell syntax only**
- **DO NOT use Unix/bash syntax**: no `&&` for command chaining, no `||`, no bash-isms
- **Use `;` (semicolon)** to chain multiple commands in PowerShell: `cmd1 ; cmd2`
- Assume Windows PowerShell for all shell commands
- Use `.\` prefix to run local executables
- Do not attempt to compile code - ask user to compile in CLion and report errors

### Environment Variables
```powershell
# Session only
$env:VAR = "value"

# Persistent (user level)
[System.Environment]::SetEnvironmentVariable('VAR','value','User')
```

### Git Operations

**CRITICAL: Always ask user before committing! Never commit without explicit permission.**

**CRITICAL: Do NOT attempt to verify commits after running `git commit`!**
- The terminal output often doesn't display properly
- Trust that the commit worked if no error was shown
- Don't run `git status`, `git log`, or `git show` to verify commits
- Just proceed with the next task after committing

**CRITICAL: Avoid commands that pause for user input!**

#### ✅ Safe Git Commands (No User Input)
```powershell
# Check status (always safe)
git status
git status --short

# Show commit history (use flags to prevent paging)
git log --oneline -5
git log --oneline --graph -10

# Show specific commit (use --no-pager or limit output)
git --no-pager log -1
git --no-pager show --stat HEAD

# Check diff (use --no-pager for safety)
git --no-pager diff
git --no-pager diff --cached
```

#### ❌ Commands That Pause for Input (AVOID in automation)
```powershell
# These will pause if output is too long:
git log              # Opens pager (less/more)
git show             # Opens pager
git diff             # Opens pager for large diffs
git log -1 --stat    # May open pager

# Use --no-pager prefix or limit output:
git --no-pager log
git log --oneline -5
```


---

## Common Tasks

### Adding a New Source File
1. Create `.cpp` and `.h` in appropriate `src/<module>/` directory
2. Add header guard using `FRANKYCPP_*` pattern
3. Add copyright header (2018-2026 Frank Kopp, MIT License)
4. File is auto-discovered by CMake globbing

### Adding a New Test
1. Create test file in `test/<module>/` directory
2. Include `<gtest/gtest.h>` and relevant headers
3. Use `TEST()` or `TEST_F()` macros
4. Tests are auto-discovered by CMake

### Modifying Configuration
1. Add new field to `SearchConfigData` or `EvalConfigData` with default value
2. Add YAML parsing in the `convert<>` specialization
3. Update `config/search.yaml` or `config/eval.yaml` with new parameter
4. Access via `ConfigManager::instance().search().NEW_FIELD`

---

## Documentation Reference

- `docs/FrankyCPP_Codebase_Review.md` - Comprehensive codebase analysis and roadmap
- `docs/Logger.md` - Logging system documentation
- `README.md` - Build instructions and version history

---

*Last updated: 2026-01-30* 
