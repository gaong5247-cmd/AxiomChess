# Axiom 0.7 — measured cost reduction, first candidate

## Outcome and scope

Implemented responsibility separation, compile-time research cost profiling and
one isolated optimization: reuse exact check/SEE facts already computed by move
ordering. `reuse_move_facts` remains **CANDIDATE / default OFF**. No new pruning,
evaluation retuning, safety-mask change, board rewrite or feature promotion.
This is a completed first candidate experiment, **not completion of every item
in the 0.7 roadmap and not a demonstrated Elo improvement**.

Production: `build/Release/axiom-0.7-cost.exe`. Research:
`build-causal/Release/axiom-0.7-cost-research.exe`. Visual Studio 18 2026,
MSVC 19.51.36246.0, C++20, Release AVX2/IPO, Windows, six logical processors.
Source is not a Git checkout; frozen ZIP hashes identify revisions.

| Frozen artifact | SHA256 |
| --- | --- |
| baseline/v06-final.exe | 6744B4A4851DBCF583F99BEDAB3CAE99C9363F41E5C7B24F6FF83EEB150B61E7 |
| baseline/v06-final-source.zip | D8DF18F7663BE41D0FE96331A2614D3056EE9F09345EE23187C9E3E1B3FDB108 |
| baseline/v07-candidate.exe | 7f404d751d30415bc70fc4644d308b4d34caea13738b21bf895ae171cf112982 |
| baseline/v07-candidate-source.zip | 299f79ca087050ee66e66d5eb77f135ac050e0637a33e2a7200ce00080a65006 |

Old binaries, source archives, unsuccessful experiments and all 0.6 results are
preserved. The candidate archive contains implementation/tests/tools; this final
report is produced afterward. Per-experiment manifests record actual binary hashes,
commands, options, budget, corpus, reference and dates. Derived manifests point to
the original measurements; they do not imply every requested measurement exists.

## 1–4. Dominant cost, board, legal generation, attacks

Research profile: five `tests/bench.fens`, 20,000 nodes each, Threads 1, Hash 32 MB,
Middlegame, ProofNodes 0, no time limit, trace disabled. **Instrumented times include
observer overhead**; use release repeated benchmarks below for performance claims.
Nested recursive inclusive times must not be summed.

Top ten exclusive instrumented times, candidate OFF:

| Scope | ms |
| --- | ---: |
| Legal generation, excluding instrumented children | 260.95 |
| attacked | 254.84 |
| push | 234.78 |
| QSearch, excluding instrumented children | 136.74 |
| gives_check, excluding instrumented children | 115.66 |
| evaluation, excluding instrumented children | 112.72 |
| pop | 112.05 |
| attack map | 96.47 |
| legal captures | 83.57 |
| pseudo generation | 71.92 |

Board: 4,274,388 push and 4,274,429 pop calls, plus 41 null pushes.
42.74388 pushes per searched node. Each snapshot copies a logical 512-byte square
array; total counted payload is 4,377,015,296 bytes, **not measured DRAM traffic**.
Push origins: legal filtering 3,001,386; gives_check 741,843; SEE 313,955;
QSearch 186,952; main search 30,252. Undo remains the original full snapshot.

Legal generation: 109,644 calls, 695.12 ms inclusive; pseudo generation 78.67 ms
inclusive. attacked: 4,143,703 calls / 254.84 ms; complete origin distributions
are in `results/v07-legalgen/manifest.json` and profile summaries. Pin detection,
special move checks and evasion filtering have not received separate timers.

## 5–9. QSearch, SEE, gives_check and ordering

Profile qnodes = 92,132 / 100,000 (92.13%). Search tree and qnode count are unchanged
by reuse. q_searched 69,775; checks 18,461; evasions 25,299; stand-pat cutoffs
37,566; SEE skips 3,438; delta skips 5,574. `q_generated` counts all generated legal
candidates, not capture-only candidates. Histograms are retained per position.
Three of five FEN exceed the 0.90 screening threshold; all are **UNKNOWN** cause,
not proven explosions. No arbitrary q-depth cap or new delta rule was added.

| Operation | OFF calls | ON calls | Change |
| --- | ---: | ---: | ---: |
| push | 4,274,388 | 4,102,804 | -4.01% |
| gives_check | 741,843 | 608,807 | -17.94% |
| SEE | 95,532 | 85,533 | -10.47% |

OFF SEE inclusive 144.99 ms; gives_check 201.76 ms. SEE call origins and internal
push/attack costs are retained; mean recursive depth is **not measured**.
Original legal, mutation-based SEE remains the reference and pruning implementation.

Ordering: 271.16 ms inclusive; scoring 258.28 ms inclusive (including checks/SEE),
sort 7.03 ms. Sort is not the dominant cost here. No staged picker or history
bucket rewrite was justified by this sample; previous partial ordering stays OFF.

## 10–14. Safety, evaluation, pawn cache and TT

Safety policy unchanged. Preserved 0.6 300-FEN / 20k-node / nine-mask study:
ALL mean CP loss 71.65; no-improving 68.49; no-PV 68.19. These are tentative
overprotection candidates, not causal node-cost estimates or promotion evidence.
All no-* masks remove bits from ALL, **not legacy**. Joint activation credit is
not independent benefit. Pair interaction traces remain research evidence;
per-reason counterfactual cost/benefit, reference saves/harms and paired tests for
mask changes are unfinished. No safety reason was weakened in this candidate.

Evaluation inclusive 327.19 ms; attack map 96.47 ms; king terms 46.68 ms;
pawn probe 72.42 ms inclusive, pawn compute 50.76 ms. Pawn hits/misses
74,458 / 28,164: 72.56% hit rate. Replacement/collision/entry-layout measurements
and individual material/PST/mobility/space/phase timings are unfinished.
The reserved strategic_eval/phase/pawn_terms profile counters are uninstrumented:
their zeros mean **unmeasured**, not free. No correlated term was removed.
Correction holdout remains the prior tiny 93.7167→93.5967 cp MAE result; no retune.

TT: 7,841 probes / 2,249 hits; proof-key construction 7,911 calls / 6.64 ms.
Proof-key timing is not context comparison timing. TT entry size/bandwidth and
compact-context experiment remain unfinished; strict identity guards are retained.

## 15–16. Architecture and decisions

`qsearch.cpp`, `move_order.cpp`, `search_report.cpp` own their respective
responsibilities. Shared internal RAII move restoration and interruption type
keep cross-file unwinding consistent. Main PVS/iteration/history remain worker-owned
Search methods; no virtual dispatch or new context pointer bundle was introduced.
Research events and detailed timers are compile-time separated from production.

Candidate: per-ply facts preserve the exact original move ordinal after sorting.
They reuse existing check results and known capture SEE values; unknown TT-move
SEE is computed normally. Scratch is allocation-once per worker when enabled;
oversized move lists fall back. Same-ply auxiliary searches precede ordering.
No caching across positions, no approximation, no move reclassification.

No new optimization was rejected as proven harmful. Deferred is not rejected:
fast SEE, direct legal generation, incremental Undo, staged picker, compact TT.
rfp, calibrated_lmr and adaptive_time remain OFF; old failed evidence is preserved.

## 17–20. Repeated benchmarks and reference quality

20 unique FEN (five bench + first 15 critical), three repetitions, alternating
OFF/ON order, same release binary, Threads 1, Hash 32, proof disabled. No deliberate
concurrent benchmark workload. Limits: 20k nodes / depth 4 / 100 ms. Reference:
Stockfish 18 depth 18 MultiPV 3 plus forced selected-move searches.

| Mode | OFF | ON | Interpretation |
| --- | ---: | ---: | --- |
| Fixed-node median time / 20 FEN | 4741 ms | 4662 ms | 1.67% lower |
| Fixed-depth median time / 20 FEN | 3929 ms | 3786 ms | 3.64% lower |
| Fixed-depth nodes / all repeats | 913,128 | 913,128 | same tree work |
| Fixed-time mean completed depth | 3.2333 | 3.2500 | small difference |
| Fixed-time nodes / all repeats | 515,674 | 533,898 | +3.53% |
| Fixed-time Top-1 / 60 observations | 12 | 12 | unchanged |
| Fixed-time Top-3 / 60 observations | 33 | 33 | unchanged |
| Mean CP loss / 57 cp observations | 39.63 | 39.63 | unchanged |
| Blunders >200cp | 0 | 0 | small sample only |

Repeated observations are **20 independent positions, not 60**. Mate-valued rows
are excluded from CP means. Reference depth 20/22 stability confirmation is not
done. Exact fixed-node OFF/ON: 123/123 including castling, promotion and pinned EP;
frozen 0.6 vs 0.7 OFF: 100/100 move/depth/nodes/score/PV agreement. Refactor-only
stage separately passed 100 positions. Fixed-node/depth changes are throughput,
not reduced search-tree size or proven strength.

## 21–24. Paired games, Elo, SPRT and time safety

Completed: smoke **9 W / 6 L / 5 D** (20 games), pilot **56 W / 30 L / 14 D**
(100 games, 63%). Both sides recorded **zero time forfeits and zero illegal moves**
in both runs. See `results/v07-paired-smoke` and `results/v07-paired`.
Runner-reported pilot Elo estimate **+92.46 ±65.95**, nominal interval
**[+26.51, +158.41]**; LOS 99.83%. This runner interval does not account for all
opening reuse, short-TC and experiment-selection limitations. **SPRT LLR +0.42**
with bounds **[-2.94, +2.94]**: no decision. The large pilot estimate relative to
the small measured throughput gain warrants independent replication, not promotion.
Smoke and pilot reuse openings/seed and are not pooled as independent evidence.
No established Elo gain or feature promotion is claimed.
Both use frozen 0.6, same 20 openings with reversed colors, Threads 1, Hash 32,
ProofNodes 0, TC 1+0.01, concurrency 2, Middlegame features; only candidate enables
reuse_move_facts. Short TC is a limitation; no long-TC claim.

External stop probes: 10 searches, OFF/ON, request after 100 ms, observed maximum
0.64 ms to bestmove. Includes IPC/scheduling, subsystem UNKNOWN; not a worst-case
bound. Fixed-time benchmark reported zero hard overshoot. TimeGuard and adaptive
time settings are unchanged; no time-management retune.

## 25–27. Correctness, unresolved risks and next work

Release 3/3 CTest (5.90 s), Debug 3/3 (46.10 s), MSVC ASan 3/3 (94.16 s),
research Release 3/3. Cached path is explicitly included in calibration tests.
UCI smoke covers finite search, concurrent readiness, multiworker experimental
search, stop, terminal position and quit. Trace-on/off determinism, parent IDs,
filter/cap/overwrite protection, safety-mask JSON, forced-root replay, correction
read-only state and ordering uniqueness tests pass. No observed regression.
MSVC ASan is not UBSan. Initial shell invocation omitted `.\` for build.ps1;
corrected Debug/Release runs are in `*-validation-final.log`; the completed ASan
log is independent of that shell invocation error.

Not completed: massive board make/unmake fuzz (no Undo change), new legal/SEE
equivalence implementations, 1000+ critical corpus, complete subsystem profile,
SEE recursion-depth distribution, full causal qsearch classification, safety
benefit/node-cost attribution, long-TC paired testing, decisive SPRT and depth
20/22 reference stabilization. Fresh subtree replay does not restore original
TT/history/stack; do not label its diagnoses causal proof.

Next highest-value work: broaden this unchanged-tree candidate's fixed-time and
paired evidence; then isolate legal-filter push cost with full correctness fixtures.
Refine fine-grained profile and check-chain diagnostics before choosing direct
legal generation or incremental Undo. Keep optimization changes separate.

## Reproduction

Run from the project directory in PowerShell. Result directories must not exist.

```powershell
.\build.ps1 -Configuration Release -NativeAVX2
.\tools\build-causal.ps1
node tests/move-facts.mjs build/Release/axiom-0.7-cost.exe results/new-equivalence.json
node tools/v07-benchmark.mjs build/Release/axiom-0.7-cost.exe results/new-benchmark
node tools/v07-profile.mjs build-causal/Release/axiom-0.7-cost-research.exe results/new-profile --feature reuse_move_facts
```

Playing baseline: UCI `Middlegame=true`; isolated candidate additionally
`Feature_reuse_move_facts=true`. `Experimental=true` enables other rejected features
and is not a playing recommendation. Detailed artifacts live in the requested
v07-* directories; PARTIAL/UNMEASURED statuses are intentional.
