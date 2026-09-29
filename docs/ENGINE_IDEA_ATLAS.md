# Axiom 2 — Engine Idea Atlas

This document tracks external open-source engine ideas at the **concept level**.
Axiom remains MIT: do not copy GPL/AGPL source text into Axiom. Reimplement ideas
independently, document the concept, and validate each change with tests and
paired engine matches before claiming playing-strength gains.

## First research set

| Engine | What to study first | Integration target |
| --- | --- | --- |
| Stockfish | LMR, singular/multicut, NMP verification, ProbCut, correction histories, move picker | unified search policy, feedback |
| Reckless | modern search layout, history families, cache/PGO discipline | search + CPU layout |
| Ethereal | clean search staging, pruning interactions, time management | architecture |
| Berserk | search selectivity, history, NNUE/search integration | search policy |
| PlentyChess | recent alpha-beta heuristics, SMP/search organization | search + SMP |
| Obsidian | selective search and modern history schemes | search feedback |
| Caissa | pruning/reduction architecture, NNUE integration | search + eval |
| Seer | NNUE/search balance, time control | eval + root |
| Koivisto | SMP, NNUE architecture, search organization | SMP + eval |
| Alexandria | modern NNUE and selective-search implementation | search + eval |
| Viridithas | Rust search architecture, original self-play tuning, history ideas | search + testing |
| Weiss | compact strong alpha-beta design | search |
| RubiChess | CPU architecture support, NNUE, SMP | hardware + SMP |
| Clover | search heuristics and NNUE integration | search |
| Stormphrax | Chess960 correctness, NNUE, NUMA/build optimization | correctness + hardware |
| Igel | NNUE, Syzygy, optimized build validation | eval + release |
| ShashChess | position-sensitive search personality concepts | context classifier |
| Arasan | mature search/time/endgame engineering | root + endgame |
| Minic | search experimentation and NNUE | search |
| Texel | tuning methodology and evaluation/search experiments | tuning |
| Laser | selective search architecture | search |
| Winter | evaluation/search research | eval + search |
| Black Marlin | modern NNUE search | search + eval |
| Velvet | compact strong engine architecture | search |
| Halogen | search heuristics | search |
| Patricia | tactical/playing-style experiments | tactical risk |
| Willow | modern engine-programming experiments | search |
| Integral | modern search/NNUE implementation | search + eval |

## Axiom design rule

External engines provide **evidence and ideas**, not code to transplant.

Every imported concept should map into one of:

1. Node context
2. Branch importance / confidence
3. Pruning policy
4. Reduction / extension policy
5. Search feedback / error attribution
6. Move ordering
7. Evaluation correction
8. SMP / time management
9. Hardware/cache layout
10. Validation and SPRT methodology

## Phase 1 implemented

`v2/policy.hpp` introduces a common policy layer. LMR, NMP, RFP, ProbCut and
singular-search margins can now consume the same node evidence instead of each
owning unrelated magic-number logic.

This is an architectural change, **not an Elo claim**.
