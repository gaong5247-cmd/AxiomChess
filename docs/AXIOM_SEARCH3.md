# Axiom Search 3 — Adaptive Selectivity

This branch changes the search policy from mostly depth/move-index driven reductions to a shared confidence-driven model.

## Core changes

- **Confidence-driven LMR**: combines PV/TT-PV state, improving/unstable evaluation, cut-node status, countermove, history, continuation history, move index and branching factor.
- **Adaptive Null Move Reduction**: reduction grows with depth, static-eval surplus over beta, and improving positions; unstable nodes buy back depth.
- **Adaptive ProbCut margin**: deeper nodes require a wider tactical margin, and unstable nodes are treated more conservatively.
- **Volatility-aware aspiration window**: the next root window expands when the previous iteration score moved sharply and contracts when it was stable.

The intent is not to make every pruning rule more aggressive. It is to spend nodes asymmetrically:
high-confidence branches are searched closer to full depth, while low-confidence late quiet moves are reduced harder.

## Required validation before merge

1. Build Release + ASan/UBSan if available.
2. Run the existing bench and record nodes/NPS/checksum.
3. Tactical regression suite: ensure no material increase in misses at fixed nodes.
4. Self-play SPRT or at least 2,000 paired games against main at 10+0.1 and 60+0.6.
5. Compare:
   - Elo / score
   - average depth and seldepth
   - LMR re-search rate
   - null verification rejection rate
   - ProbCut cutoff accuracy
   - aspiration fail-high/fail-low count
   - time overshoot
6. If strength is neutral but nodes fall materially, retest at equal time rather than equal depth.

## Tuning knobs

The highest-leverage constants are intentionally centralized in the new helpers at the top of `src/search.cpp`:
`branch_confidence`, `adaptive_lmr`, `adaptive_null_reduction`, and `adaptive_probcut_margin`.

Tune these with paired matches; do not infer strength from NPS alone.
