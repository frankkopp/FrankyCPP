Do not try to compile or run tests as you do not have the Clion environment and it would fail anyway. Ask me and I will run the task and provide the result.

done: Add a configurable Move Overhead option (already proposed as Step 1)
done: Rationale: replace hardcoded 20ms/5ms with a UCI option used in both movetime and remaining‑time modes.
done: Prompt: Please implement Step 1: add a UCI Move Overhead option and use it in setupTimeControl across movetime and remaining‑time modes, touching src/engine/SearchConfig.h, src/engine/UciOptions.cpp, and src/engine/Search.cpp. I will build and run.

done: Soft time guard before expensive re‑searches (Step 2)
done: Rationale: avoid starting aspiration expansions, PV re‑searches, IID, or full re‑searches when time is almost up.
done: Prompt: Please add an isTimeAlmostUp() helper and call it at re‑search trigger points in src/engine/Search.cpp (root PVS re‑search, LMR re‑search, aspiration expansion, IID). I will build and run.

done: Adaptive iteration duration predictor
done: Rationale: replace fixed 1.5× last iteration heuristic with a predictor using node growth factor and current NPS to estimate next iteration cost.
done: Prompt: Please replace the 1.5× heuristic in iterativeDeepening with an ETA based on last iteration nodes and current NPS in src/engine/Search.cpp. I will build and run.

done: Panic time extension on volatility
done: Rationale: add extra time when fail‑low, big eval swings, or checks at root indicate tactical complexity.
done: Prompt: Please add a volatility detector (fail‑low, |Δeval| threshold, root in‑check) and call addExtraTime() conservatively in src/engine/Search.cpp. I will build and run.

done: Complexity‑aware time allocation
done: Rationale: spend more time when root move count is high, position is in check, or many legal captures exist; spend less on trivial or forced positions.
done: Prompt: Please weight per‑move budget by root complexity indicators (legal move count, in‑check, captures ratio) in setupTimeControl and root iteration gating in src/engine/Search.cpp. I will build and run.

done: Higher‑precision timer tail
done: Rationale: reduce overshoot on Windows by busy‑waiting the last few milliseconds instead of sleeping.
done: Prompt: Please modify startTimer() in src/engine/Search.cpp to switch from sleep to a short busy‑wait for the final ~2–3ms before deadline. I will build and run.

done: Improved movesLeft model
done: Rationale: estimate moves to go using game phase, material, and repetition risk rather than a linear factor.
done: Prompt: Please refactor movesLeft estimation in setupTimeControl in src/engine/Search.cpp to use phase/material buckets with tunables in src/engine/SearchConfig.h. I will build and run.

Root single‑move fast path
Rationale: if only one legal root move, skip deep search or cap depth/time.
Prompt: Please add a single‑move early exit in iterativeDeepening with minimal verification in src/engine/Search.cpp. I will build and run.


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

Quiescence bailout under time pressure
Rationale: cap qsearch depth or switch to stand‑pat only when time is almost up.
Prompt: Please add a time‑pressure guard in qsearch to early‑return (stand‑pat) when isTimeAlmostUp() in src/engine/Search.cpp. I will build and run.

Emergency move mode
Rationale: when remaining time below a threshold, enforce a very fast, shallow search to avoid flagging.
Prompt: Please add a UCI Emergency Move Time threshold and, when triggered, cap depth and avoid re‑search paths in src/engine/Search.cpp. I will build and run.

Time‑aware feature shedding
Rationale: disable or reduce costly features (IID, large LMR re‑search, history updates) under time pressure.
Prompt: Please conditionally skip IID and limit LMR re‑search when isTimeAlmostUp() in src/engine/Search.cpp. I will build and run.

Enhanced timing telemetry
Rationale: log computed budgets, reserves, extra time changes, iteration ETAs, and overruns for tuning.
Prompt: Please enrich timing logs in src/engine/Search.cpp and add a SearchConfig flag to toggle verbose timing telemetry. I will build and run.

Ponder time cap option
Rationale: avoid runaway ponder by capping max ponder time per move.
Prompt: Please add UCI Max Ponder Time and enforce it in startTimer()/ponderhit() paths in src/engine/Search.cpp. I will build and run.

Min/Max per‑move clamps (UCI options)
Rationale: prevent pathological allocations by clamping computed budget.
Prompt: Please add UCI options Min Move Time and Max Move Time and clamp the computed timeLimit in src/engine/Search.cpp and wire in src/engine/UciOptions.cpp. I will build and run.

Files you’ll likely touch for these steps:
src/engine/Search.cpp
src/engine/SearchConfig.h
src/engine/UciOptions.cpp
src/engine/SearchLimits.h
