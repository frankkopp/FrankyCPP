# Optimize — Code Simplification & Cleanup Review

> **Usage in Copilot Chat (CLion):**
>
> Reference this file and the target files, then state parameters:
> ```
> #file:.github/prompts/optimize.prompt.md
> Analyze #file:src/engine/Search.cpp — max 15 findings, focus: performance
> ```
>
> For cross-file analysis:
> ```
> #file:.github/prompts/optimize.prompt.md
> Analyze #file:src/engine/Search.cpp #file:src/engine/Search.h #file:src/engine/PlyInfo.cpp — max findings: all, focus: redundancy
> ```
>
> **Usage in Claude.ai Chat (with Filesystem access):**
>
> Reference the prompt directly and specify scope. Output is written to `docs/specs/`
> as a plan document. See `PLAN_Code_Simplifications.md` as a style reference.

---

## Task

Perform a code simplification and cleanup review on the provided files. Produce a **prioritized list of findings only**. Do not modify code, do not commit.

## Parameters (set by user in the chat message)

| Parameter        | Values                                                                                                                  | Default      |
|------------------|-------------------------------------------------------------------------------------------------------------------------|--------------|
| **Scope**        | Files or directories to analyze                                                                                         | *(required)* |
| **Max findings** | Number or `all`                                                                                                         | `15`         |
| **Focus**        | `performance`, `redundancy`, `dead_code`, `readability`, `idioms`, `type_safety`, `headers`, `chess_patterns`, or `all` | `all`        |

---

## Analysis Categories

### 1. PERFORMANCE (hot path)
- Unnecessary copies, allocations, temporaries in hot code
- Missing `constexpr`, `const`, `inline`, `[[nodiscard]]` where the compiler benefits
- Suboptimal data structures or algorithms for the access pattern
- Branch-heavy code → branchless alternatives
- Cache-unfriendly access patterns (pointer chasing, scattered memory)
- Missed intrinsic opportunities (popcount, bitscan, prefetch, SIMD)
- Virtual calls or indirect branches in hot paths
- Unnecessary function call overhead (consider forced inlining)
- Pass-by-value vs pass-by-reference mismatches for the type size

### 2. REDUNDANCY
- Duplicate or near-duplicate code within a file
- Duplicate or near-duplicate code across files → suggest extraction into shared utility
- Copy-pasted logic with minor variations → generalize via templates or parameters
- Wrapper functions that add no value

### 3. DEAD CODE
- Unused functions, methods, variables, parameters, includes, forward declarations
- Unreachable branches (always-true/always-false conditions)
- Stale commented-out code blocks (ignore single-line debug comments)
- Unused enum values, type aliases, constants

### 4. READABILITY & MAINTAINABILITY
- High cyclomatic complexity, deep nesting (>3 levels)
- Functions too long → decompose
- Magic numbers/strings → named constants or enums
- Inconsistent or misleading naming
- Missing comments on non-obvious logic

### 5. MODERN C++20 IDIOMS
- Pre-C++17/20 patterns with cleaner modern equivalents
- Raw pointers where references/smart pointers fit (NOT in perf-critical data structures where raw pointers are intentional)
- C-style casts → `static_cast`/`reinterpret_cast`
- Manual loops → algorithms/ranges (only where performance-neutral)
- Missing structured bindings, `std::optional`, `if constexpr`, concepts, `std::span`
- Old `enum` → `enum class`
- Macros → `constexpr`/`consteval`/templates (except SPSA tuning macros)

### 6. TYPE SAFETY
- Raw `int` for squares, pieces, colors, moves → enums or strong types
- Implicit narrowing conversions
- Missing explicit casts

### 7. HEADER HYGIENE
- Unnecessary or transitively-relied-upon includes
- Missing forward declarations
- Implementation details leaking into headers

### 8. CHESS ENGINE — MISSING PATTERNS
Identify established high-performance chess programming patterns the code does **not** use but could benefit from:
- Incremental updates (Zobrist, material, PST, NNUE accumulator) where code recomputes from scratch
- Staged move generation vs generating all moves upfront
- Lazy sorting (pick-best loop) vs full sort
- Prefetch hints for TT probes
- Template specialization by color/piece/node-type to eliminate runtime branching
- Any other technique from competitive chess programming that applies

Flag as **opportunities**, not defects. Explain the expected benefit.

### Chess Engine Patterns to Respect
Do NOT flag these as problems — they are intentional:
- Lookup tables with magic numbers (Zobrist keys, magic multipliers, PST, reduction tables)
- Dense bit manipulation chains (popcount, bitscan, shifts, XOR)
- Global/static mutable state for TT, history tables, thread-local search stacks
- Macro-based tuning parameters (SPSA workflow)
- Manual loop unrolling or code duplication for performance (flag only if a cleaner alternative is equally fast)
- `goto` in hot loops if used for performance

---

## Output Format — Plan Document

Write the output as a **plan document** saved to `docs/specs/`. The document must be
self-contained and suitable for iterative refinement and implementation across multiple
sessions (potentially by a different AI or human).

Use `docs/specs/PLAN_Code_Simplifications.md` as a **style reference** — it shows the
general tone, level of detail, and use of before/after code snippets. Adapt the structure
to what makes sense for the findings. Not every section is required for every analysis.

### Required Elements

Every plan document must include at minimum:

1. **Header** with title, date, status (`📋 PLANNING`), and scope
2. **Goal** — one paragraph describing the objective
3. **Summary Table** — overview of all findings with columns for: ID, short description,
   file(s), category, severity, confidence, estimated effort, and risk
4. **Detailed Proposals** — one section per finding, each containing:
   - Location (file and line range)
   - Category and severity
   - Problem description (what and why)
   - Concrete suggestion with before/after code snippets where helpful
   - Risk assessment (flag any search/eval behavior changes explicitly)
   - For performance claims: explanation of WHY it is faster
5. **Prioritized implementation order** — numbered list, dependencies noted
6. **Verification notes** — how to confirm correctness (bench signature, tests, etc.)

### Optional Elements (include when useful)

- Guiding principles (if scope-specific constraints apply beyond the general rules)
- Items considered and rejected (with rationale — prevents re-analysis of dead ends)
- Cross-references to related plan documents
- Estimated total impact (lines saved, performance gain, etc.)

### Document Naming Convention

Use: `PLAN_[Scope]_[Focus].md`

Examples:
- `PLAN_Search_Performance.md`
- `PLAN_Eval_Redundancy.md`
- `PLAN_Chesscore_Idioms.md`
- `PLAN_Codebase_DeadCode.md`

### Relationship to Existing Plans

Before creating a new document, check `docs/specs/` for existing plans covering the same scope.
If an existing plan overlaps:
- **Extend** the existing document if the focus is the same or closely related.
- **Create a new document** if the focus is substantially different (e.g., existing plan covers
  redundancy, new analysis covers performance).
- **Cross-reference** between documents when findings relate to each other.

---

## Rules

1. **Findings only** — do NOT modify code, create files outside `docs/specs/`, or commit.
2. **Never sacrifice performance** in hot-path code, even marginally. If uncertain whether code is hot path, assume it is and flag uncertainty.
3. **No new external dependencies** without explicit flagging.
4. **No project restructuring** (file moves, directory reorg) unless asked.
5. **No style-only findings** (whitespace, brace placement).
6. **C++20 features only** — verify availability before suggesting.
7. **Conflicting findings** (readability vs performance): present both, let the user decide.
8. **Be specific** — exact code, exact problem, exact fix. *"This could be improved"* is not a finding.
9. **When uncertain about chess-engine-specific intent, say so.** Do not guess whether a pattern is intentional.
10. **The plan document must be actionable.** Another session must be able to pick it up and implement without re-analyzing the code.
