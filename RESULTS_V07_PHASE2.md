# Axiom 0.7 Phase 2 — legal filtering candidate C2

## Outcome / scope

Implemented **TYPE A `legal_fast_path`**, default OFF. It removes redundant
legality board mutations without changing move order, pruning, evaluation or
QSearch policy. Full snapshot Undo and old legal generator remain available.
This report covers the first primary-target candidate and replication work,
**not completion of the entire 63-section Phase 2 research roadmap**.

Observed: legal-filter push -73.78%, total push -51.80%, attacked -50.79%;
fixed-depth time -14.49% with identical nodes. Reference metrics are mixed:
mean CP loss improves, Top-1 improves, Top-3 declines. Therefore no promotion.
Speed alone is not playing strength.

## Frozen identities and comparisons (items 1, 20–21)

| Artifact | SHA256 |
| --- | --- |
| A: baseline/v06-final.exe | 6744B4A4851DBCF583F99BEDAB3CAE99C9363F41E5C7B24F6FF83EEB150B61E7 |
| B binary: baseline/v07-phase1.exe | 7F404D751D30415BC70FC4644D308B4D34CAEA13738B21BF895AE171CF112982 |
| B source: baseline/v07-phase1-source.zip | 299F79CA087050EE66E66D5EB77F135AC050E0637A33E2A7200CE00080A65006 |
| C binary: baseline/v07-phase2.exe | 79BF757B7911F6AB2B61E8F2CE929EB6260B0572CE3B3271F896C36CE4948800 |
| C source: baseline/v07-phase2-source.zip | 891B47E49D3303C96308943753CA33DC937354C7DE6CE1C7660D079C82E0AFDB |

B playing candidate means reuse_move_facts ON; C2 means legal_fast_path ON and
reuse_move_facts OFF. C2 benchmarks compare the **same C binary**, legal OFF/ON.
The C OFF vs frozen B binary OFF fixed-node comparison passed 100 positions.
The final contemporaneous A/B/C benchmark is recorded separately in
`results/v07p2-abc`; different feature configurations and historical timing runs
must not be conflated.
No combined candidate is implemented or recommended yet.

VS 18 2026 Community / MSVC 19.51.36246.0 / C++20 / Release AVX2 IPO, Windows,
six logical processors. No Git revision: source ZIP identity is used. Archives
are never overwritten. Final documentation and derived manifest generator were
written after the candidate source snapshot; the playing implementation is frozen.
Later research-only legal-class instrumentation is in
`baseline/v07p2-attribution-source.zip`, SHA256
`01B64CC4B6CB3D972633BCB805C0D81370BA0561DBFB1DCE1A963EA22A0431CB`.
It is compiled out of production; the playing binary above is unchanged.

## Legal fast path and correctness (items 2–6, 29)

Each legal generation computes the current king/check state. Checked positions
and missing/invalid kings fall back to the full original generator. Otherwise,
eight king rays identify the sole friendly blocker followed by a matching enemy
rook/bishop/queen: absolute pins. These facts live only inside the immutable
generation call; no cache survives push/pop.

Fast acceptance requires non-king, unpinned, non-EP. A stationary king cannot
become newly attacked by a knight/pawn/king merely because an ordinary friendly
non-king moves; a newly opened slider ray requires removing its sole blocker,
which pin detection excludes. Ordinary captures replace destination occupancy.
Promotion changes our destination piece but preserves the relevant occupancy.
EP removes an additional square and is always slow. King moves/captures/castling
retain old push/check/pop semantics. Pinned moves are tested, not blindly rejected.

`Board::legal_moves_reference()` retains the original full-filter algorithm.
`Board::legal_fast_path` is an execution policy, not position identity; run_single
sets it explicitly from Limits for each worker. It does not change hash/history.

Validation:

- 1,000,000 **sampled, not asserted unique** positions, seed 2026091307.
- Exact ordered vector equality, stricter than sorted move-set equivalence.
- 997,605 recorded makes, 8,499 sequences up to 160 plies; all undone.
- Restored squares, FEN (side/rights/EP/clocks), hash, history, identities and
  proof_key. King location is represented by board squares, not a separate cache.
- Fixtures: castling, pinned EP, capture promotion and all underpromotions,
  double check, absolute pins, mate, stalemate, 99-halfmove state, start perft4.
- Start perft4 = 197,281. Existing perft and rule audits remain in CTest.
- 123 OFF/ON searches: bestmove, score/type, depth, nodes, PV all identical.
- C OFF vs B OFF: 100 exact fixed-node comparisons.
- Release 4/4 (8.89s), Debug 4/4 (48.29s), ASan 4/4 (101.85s), research 4/4.
- UCI concurrent readiness, experimental multiworker stop and terminal tests pass.
  Trace determinism/filter/cap/parent tests also pass.

No observed correctness mismatch. Random sampling is not a formal exhaustive
proof; one million samples are not one million independent games. MSVC ASan
does not imply UBSan. No incremental Undo was implemented: the secondary candidate
was not needed to measure this substantial primary-target gain.

## Profile: what was actually eliminated (items 2, 4–5, 9)

Same five diagnostic FEN, 20k search nodes each, Threads1, Hash32, Middlegame,
ProofNodes0, no time limit, research timing enabled but trace disabled.

| Metric / 100k search nodes | OFF | C2 ON | Reduction |
| --- | ---: | ---: | ---: |
| Legal-filter pushes | 3,001,386 | 787,055 | 73.78% |
| Total pushes | 4,274,388 | 2,060,057 | 51.80% |
| Pushes / node | 42.74388 | 20.60057 | 51.80% |
| attacked calls | 4,143,703 | 2,039,016 | 50.79% |
| SEE calls | 95,532 | 95,532 | 0% |

Thus **2,214,331 of the old legal-filter pushes were unnecessary for this sample**.
The remaining slow checks include king moves, checked positions, pins and EP;
they have not been proven irreducible. Full snapshot push/pop remains elsewhere.
Complete attack call-site counts are in `results/v07p2-attacks/manifest.json`.

Research inclusive times include instrumentation overhead. Nested `Legal` scopes
count both a fast-path dispatch and checked-position fallback; legal scope counts
are **not unique generation counts**, and nested inclusive times cannot be added.
Do not use instrumented timings as the wall-clock speedup claim.
Follow-up research classification completed on the same five-FEN/100k-node budget:

| Class | Pseudo candidates | Legal | Illegal | OFF pushes | ON pushes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Pawn | 655,577 | 580,629 | 74,948 | 655,577 | 80,985 |
| Knight | 407,205 | 368,873 | 38,332 | 407,205 | 41,808 |
| Bishop | 308,973 | 277,742 | 31,231 | 308,973 | 34,085 |
| Rook | 484,413 | 400,385 | 84,028 | 484,413 | 88,532 |
| Queen | 660,665 | 608,971 | 51,694 | 660,665 | 57,092 |
| King | 484,553 | 363,249 | 121,304 | 484,553 | 484,553 |
| Capture | 241,822 | 217,139 | 24,683 | 241,822 | 51,491 |
| Quiet | 2,759,564 | 2,382,710 | 376,854 | 2,759,564 | 735,564 |
| Promotion | 26,828 | 24,408 | 2,420 | 26,828 | 2,420 |
| Castling | 17,597 | 17,597 | 0 | 17,597 | 17,597 |
| EP | 488 | 312 | 176 | 488 | 488 |
| In check | 408,596 | 83,567 | 325,029 | 408,596 | 408,596 |
| Not in check | 2,592,790 | 2,516,282 | 76,508 | 2,592,790 | 378,459 |
| Pinned | 24,338 | 6,651 | 17,687 | 24,338 | 24,338 |
| Unpinned | 2,977,048 | 2,593,198 | 383,850 | 2,977,048 | 762,717 |

All categories have identical generated/legal/illegal totals OFF/ON. Categories
overlap; do not sum across dimensions. Per-move attacked calls equal slow pushes
here. Pseudo-generation attack queries and node setup are not attributed to one
move. Complete per-class times and attack counts are in
`results/v07p2-legal/classification.json`. Example instrumented filtering times:
quiet 540.90→193.27ms, king 106.49→110.47ms. These include observer overhead and
exclude pseudo generation/pin/check setup; they are not release speedup estimates.
Research classification build: CTest4/4 and UCI/trace checks passed.

## QSearch, ordering, safety and history (items 7–15)

100 balanced FEN, 20k nodes each, C2 ON:

| Qratio statistic | Value |
| --- | ---: |
| Median | 88.505% |
| P75 | 91.385% |
| P90 | 96.155% |
| P95 | 97.265% |
| Maximum | 99.085% |

37 positions exceed 90%; their FEN, qply histogram, check and evasion counters are
saved in `results/v07p2-qsearch/summary.json`. Labels are UNKNOWN: a high ratio does
not distinguish necessary tactics from wasted work. The former five-position
92.13% ratio was not representative of the median here. At equal node/depth
budgets, qnodes are unchanged by C2; no tactical work is intentionally removed.

QSearch already consumes Phase1 check/SEE reuse when that separate flag is ON.
C2 does not alter SEE, capture ordering, delta thresholds or checks/promotions.
Cutoff-index distribution, QSearch chain attribution and a dedicated tactical
holdout remain unfinished. No arbitrary q-depth cap was introduced.

Safety consensus/weakening, LMR false-negative sampling, history saturation and
calibration curves, continuation overlap, static-vs-search error classification,
eval term ablation and a high-confidence quiet-reference corpus are **not completed**.
Existing 0.6 evidence is preserved, but no inferred causal node savings or new
TYPE B benefit is reported. Exact ordered move-set preservation includes quiet
moves, but is not a substitute for the requested quiet tactical holdout.

## Repeated release benchmark (items 16–19)

20 unique positions, three repetitions, alternating OFF/ON order; same binary,
Threads1, Hash32, Middlegame, no proof. No intentional concurrent CPU-heavy job.
Limits 20k nodes / depth4 / 100ms. Raw results include moves, scores and PVs.

| Metric | OFF | C2 ON |
| --- | ---: | ---: |
| Fixed-node totals per repetition, ms | 4528 / 4596 / 4783 | 3841 / 3808 / 3912 |
| Fixed-node median, ms | 4596 | 3841 (-16.43%) |
| Fixed-depth totals per repetition, ms | 3836 / 3680 / 4096 | 3132 / 3374 / 3280 |
| Fixed-depth median, ms | 3836 | 3280 (-14.49%) |
| Fixed-depth nodes, all repetitions | 913,128 | 913,128 |
| Fixed-depth qnodes | 842,187 | 842,187 |
| Fixed-time mean completed depth | 3.2000 | 3.4833 |
| Fixed-time nodes, all repetitions | 525,730 | 613,747 (+16.74%) |
| Fixed-time Top1, observations | 12/60 | 15/60 |
| Fixed-time Top3, observations | 33/60 | 30/60 |
| Mean CP loss, cp observations | 39.63 | 34.74 |
| Loss >200cp | 0 | 0 |

Reference is Stockfish18 depth18 MultiPV3 plus forced selected moves. Repeats
are 20 positions, **not 60 independent positions**. Three mate-valued observations
are excluded from each CP mean. Additional >50/100/200/400cp counts are in
`results/v07p2-fixed-time/tail-loss.json`; mate miss/allowed are not classified.
Depth20/22 stability confirmation and full critical/quiet holdouts are pending.

This is a throughput gain, not improved node allocation: the same-depth tree is
identical. Top3 decline means reference non-regression is not established. Neither
C2 nor reuse_move_facts is promoted; there is no combined gain to add together.

Two unique fixed-time decisions changed (all three repetitions agreed): benchmark
index3 reached depth3 instead of2 and selected c4c5 instead of g1h1; depth18 forced
reference scores were -274 vs -406cp, a 132cp recovery. Index8 reached depth4
instead of3 but selected c1f4 instead of b1c3; forced reference scores were -10
vs +29cp, a 39cp deterioration. These are useful depth-transition holdout cases,
not proof of a particular LMR/evaluation cause. Their FEN and observations are
in `results/v07p2-fixed-time/changed-decisions.json`.

## Playing tests / replication (items 1, 22–28)

Completed C2 vs frozen A results:

| Run | W / L / D | Score | Runner Elo ± interval half-width | LOS | LLR |
| --- | --- | ---: | --- | ---: | ---: |
| 20-game smoke, 1+0.01 | 7 / 9 / 4 | 45% | -34.86 ±144.80 | 30.74% | -0.04 |
| 100-game pilot, 1+0.01 | 37 / 46 / 17 | 45.5% | -31.35 ±62.94 | 16.04% | -0.17 |
| 20-game medium smoke, 5+0.05 | 7 / 11 / 2 | 40% | -70.44 ±158.55 | 16.74% | -0.07 |

Pilot nominal runner interval [-94.29,+31.59] crosses zero. All SPRTs are
**INCONCLUSIVE**; results favor the reference, not C2. All sides/runs have zero
time forfeits and illegal moves. The medium test is smoke-sized, not evidence
of long-TC strength. No pooled estimate is used across repeated openings.
**C2 does not pass playing-strength promotion: it remains OFF.**

Independent Phase1 B reuse replication against frozen A completed **300 games:
115 W / 123 L / 62 D**, score **48.67%**. Runner Elo **-9.27 ±35.15**,
nominal interval **[-44.42,+25.88]**, LOS **30.20%**, LLR **-0.18**:
**SPRT INCONCLUSIVE**. Both sides had zero time forfeits/illegal moves.
The earlier 100-game 63% result **did not reproduce**. It remains an earlier
pilot observation, not established +92 Elo. reuse_move_facts remains OFF.

All use Threads1, Hash32, ProofNodes0, matched Middlegame features, reversed colors,
concurrency2. Replication openings are 20 new legal positions with zero exact FEN
overlap with Phase1, reversed source ordering and new seed2026091302. Opening
families can still overlap. Do not pool the old Phase1 100 games into replication.
SPRT H0=0/H1=5 Elo, alpha=beta=.05, bounds +/-2.94. LLR is reported separately
from runner Elo, interval and LOS. No default-on or Elo claim from a pilot.

## Remaining work / next candidate (items 29–30)

Core legal fast-path correctness passed the measured gates, but broader tactical
quality and decisive strength evidence remain open. The best next work is the
mixed fixed-time positions and independent quiet/tactical holdout, followed by
using the measured piece-class attribution. Do not respond to a disappointing pilot by mixing
Safety/LMR retuning into C2. Incremental Undo is secondary after this measured
73.78% reduction in legal mutations. TYPE B experiments remain separate.

Every requested result directory contains evidence or an explicit deferred status;
the presence of a directory is not a claim that its experiment was performed.

## Reproduce

```powershell
.\build.ps1 -Configuration Release -NativeAVX2
.\build\Release\axiom_legal_fast.exe 1000000
node tests/move-facts.mjs build/Release/axiom-0.7-cost.exe results/new-legal-equivalence.json legal_fast_path
node tools/v07-benchmark.mjs build/Release/axiom-0.7-cost.exe results/new-legal-bench legal_fast_path
```

CLI candidate: `--middlegame --feature legal_fast_path`. UCI candidate:
`Middlegame=true`, `Feature_legal_fast_path=true`. Default remains OFF.
`Experimental=true` enables unrelated experimental/rejected flags and is not a
recommended playing preset. Result paths reject overwrites.
