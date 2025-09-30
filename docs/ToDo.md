Do not try to compile or run tests as you do not have the Clion environment and it would fail anyway. Ask me and I will run the task and provide the result.

# CMake:
- add install target rules for config files and books
- make compilation outside MSVC environment work (find vcpkg automatically)
- 


# Logging:
Here is the same small‑steps refactor plan, with updated prompts that explicitly state you will build and run tests and provide the results. I will not attempt to compile or run.

Time‑aware feature shedding
Rationale: disable or reduce costly features (IID, large LMR re‑search, history updates) under time pressure.
Prompt: Please conditionally skip IID and limit LMR re‑search when isTimeAlmostUp() in src/engine/Search.cpp. I will build and run.

Quiescence bailout under time pressure
Rationale: cap qsearch depth or switch to stand‑pat only when time is almost up.
Prompt: Please add a time‑pressure guard in qsearch to early‑return (stand‑pat) when isTimeAlmostUp() in src/engine/Search.cpp. I will build and run.

Low prio:
NPS‑based dynamic budget tracking
Rationale: track NPS during the search and estimate whether the next iteration (or re‑search) can finish within remaining time.
Prompt: Please track rolling NPS and use it to gate starting the next iteration and re‑searches in src/engine/Search.cpp. I will build and run.

Increment‑aware spend policy
Rationale: always leave a reserve, spend a fraction of increment, and avoid burning base time when increment is high.
Prompt: Please modify remaining‑time budgeting in setupTimeControl to allocate baseShare + k * increment with a fixed reserve; add tunables in src/engine/SearchConfig.h. I will build and run.

Ponder credit budgeting
Rationale: if the pondered move is played, reuse part of ponder time as credit for the move; otherwise decay.
Prompt: Please track ponderCreditMs and, on ponderhit() in src/engine/Search.cpp, add a bounded credit to extraTimeMs. I will build and run.

Decimated time checks inside hot loops
Rationale: add a very cheap periodic time check (e.g., every N nodes) to stopConditions() using an atomic deadline to minimize overhead.
Prompt: Please add an atomic deadlineNs, update it when time changes, and check it every N nodes in stopConditions() and key loops in src/engine/Search.cpp. I will build and run.

Emergency move mode
Rationale: when remaining time below a threshold, enforce a very fast, shallow search to avoid flagging.
Prompt: Please add a UCI Emergency Move Time threshold and, when triggered, cap depth and avoid re‑search paths in src/engine/Search.cpp. I will build and run.

Enhanced timing telemetry
Rationale: log computed budgets, reserves, extra time changes, iteration ETAs, and overruns for tuning.
Prompt: Please enrich timing logs in src/engine/Search.cpp and add a SearchConfig flag to toggle verbose timing telemetry. I will build and run.

Ponder time cap option
Rationale: avoid runaway ponder by capping max ponder time per move.
Prompt: Please add UCI Max Ponder Time and enforce it in startTimer()/ponderhit() paths in src/engine/Search.cpp. I will build and run.

Min/Max per‑move clamps (UCI options)
Rationale: prevent pathological allocations by clamping computed budget.
Prompt: Please add UCI options Min Move Time and Max Move Time and clamp the computed timeLimit in src/engine/Search.cpp and wire in src/engine/UciOptions.cpp. I will build and run.
