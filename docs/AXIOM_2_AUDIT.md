# Axiom 2 audit — baseline 49e12cac96dcc9d6e3a5e65ef1c9a0c818ab17fb

Source audit, 2026-09-25. Classifications apply to the new executable; all old
sources remain buildable as the independent reference engine and rule oracle.
No measured speed or strength improvement is implied by this audit.

| Area | Decision | Evidence / replacement |
|---|---|---|
| Board representation | REWRITE | `chess.hpp`: 128 integer 0x88 cells; new 64-square mailbox plus piece/side bitboards |
| Legal generation | REWRITE | `chess.cpp`: vector allocation, reference make/test/unmake; pin and check masks in new core |
| Reference generator | KEEP | `legal_moves_reference` is the differential oracle |
| Make/unmake | REWRITE | Undo copies 512-byte board; replace with compact delta undo |
| Zobrist | REWRITE | `hash()` scans all cells; incremental piece/state XOR |
| Repetition | REWRITE | per-node allocated identity strings; fixed history keys, irreversible/null boundaries |
| 50/75 moves | KEEP | preserve mate precedence and distinguish claims from automatic draws in tests |
| Search / PVS | REWRITE | `search.cpp` mixes research instrumentation, safety policy and scheduling; independent PVS worker |
| TT | REWRITE | string context, modulo index, mutex per stripe; compact aligned power-of-two clusters, synchronized access |
| Ordering | REWRITE | repeated gives_check and recursive SEE per candidate; fixed scored lists, histories |
| Killer/main history | KEEP | concept only; thread-local bounded gravity updates |
| Continuation/capture history | KEEP | concept only; compact indexed arrays |
| SEE | REWRITE | recursive legal capture enumeration in `evaluation.cpp`; bitboard exchange simulation |
| QSearch | REWRITE | all legal moves allocated, repeated check probes; fixed legal list, tactical filtering |
| Null move | KEEP | concept with material gates and verification; new independent implementation |
| LMR | KEEP | concept with recovery search; precomputed depth/index table |
| Pruning | REWRITE | explicit independent switches and counters; conservative defaults |
| Singular | KEEP | excluded-move concept; bounded extensions, no recursive exclusion |
| Evaluation | REWRITE | new tapered incremental material/PST plus inexpensive classical terms |
| SMP | REWRITE | Lazy SMP concept retained, shared TT, independent worker state |
| Time manager | REWRITE | soft/hard limits plus instability, ponder-safe deadlines |
| UCI | REWRITE | `main.cpp` rejects ponder/searchmoves and lacks MultiPV/overhead |
| MultiPV | REWRITE | independent root exclusion passes, single-PV fast path |
| Ponder | REWRITE | asynchronous stop/ponderhit lifecycle |
| searchmoves | REWRITE | validated legal root restriction |
| Syzygy | OPTIONAL | old optional Fathom root DTZ adapter; new direct bitboard adapter when configured |
| Proof solver | OPTIONAL | preserve old certificate solver separately; no unproven result labeled proven |
| Tests/perft | KEEP | existing tests plus cross-engine fuzz and new regression suite |
| Benchmark | REWRITE | fixed corpus, per-position results and deterministic checksum |
| Thread safety | REWRITE | atomics for cancellation, synchronized TT, joined lifetime, serialized UCI output |
| Research traces/calibration/retrograde | REMOVE | exclude from new executable hot path, retain old sources |
| BMI2/PEXT/AVX2 | OPTIONAL | portable bitboard implementation first; do not require CPU features |

## Bottlenecks

Structural performance hazards: full board copy per make/unmake; full hash scan;
proof_key strings with reversible history copied at every search node and TT
probe; heap move/PV lists; recursive legal SEE; repeated checks in ordering.
These are code-inspection findings, not profiler-derived percentages.

Search-quality hazards are different: expensive per-node machinery limits depth
at fixed time; unconditional opening ordering priors, broad safety exemptions,
fixed aspiration width and lack of decision-cost scheduling need controlled
ablation. Node count alone cannot establish playing strength.

## Architecture decision

New `v2/` contains types/attacks, position/incremental state, legal generation,
evaluation/SEE, TT, histories/ordering, PVS, qsearch, root scheduling,
time manager, thread manager, UCI, benchmark and independent tests. Original
`src/` and `include/axiom/` remain unchanged for reference and comparison.
Experimental decision-impact, refutation and more speculative pruning remain
off by default until arena evidence supports enabling them. No training,
network inference, model weights or opening book are introduced.
