# AxiomChess 2.0 — build and run

The new executable is independent of `axiom-1.0`; legacy code remains a rule
oracle and comparison engine. No network, neural model or opening book is used
by `axiom-2`.

## Windows / MSVC x64

From a Visual Studio developer terminal with CMake 3.25+:

```powershell
cmake -S . -B build-v2 -A x64
cmake --build build-v2 --config Release --parallel 4
ctest --test-dir build-v2 -C Release --output-on-failure
build-v2/Release/axiom-2.exe
```

If the terminal contains both `Path` and `PATH`, prefix the build commands with
`node tools/msvc.mjs`, which normalizes key casing. This is a Windows launcher
workaround and does not alter compiler optimization flags. `/EHsc` is explicit
because cancellation depends on C++ stack unwinding.

```powershell
node tools/v2-uci-test.mjs build-v2/Release/axiom-2.exe results/uci.json
build-v2/Release/axiom-2.exe --bench 8 100000
build-v2/Release/axiom-2.exe --bench 8 100000 DecisionImpact RefutationSearch
build-v2/Release/axiom-2.exe --perft 5
```

Bench uses five fixed positions, fresh Hash=32, Threads=1, deterministic node
counts and checksums. Wall time varies with the machine/load. Output includes
each best move, score, completed depth, nodes, time, NPS, hashfull and counters.
`stats main_worker` UCI diagnostics describe the main worker only; UCI node
counts include all SMP workers.

## UCI

Supported: `uci`, `isready`, `ucinewgame`, `position startpos|fen ... moves ...`,
`setoption`, `go`, `stop`, `quit`, `ponderhit`. Limits: `wtime`, `btime`, `winc`,
`binc`, `movestogo`, `movetime`, `nodes`, `depth`, `mate`, `infinite`, `ponder`,
`searchmoves`. `go mate` requests a bounded search; it is not a proof certificate.

Options: Hash 1–4096 MB, Threads 1–64, MultiPV 1–256, Ponder, Move Overhead (default 50 ms),
SyzygyPath, Clear Hash, and independent search switches advertised by `uci`.
Default single-PV search does not pay for extra MultiPV root passes.
Only standard chess is supported, not Chess960.

Default ON: LMR, NullMove, Singular, ReverseFutility, Futility, SEEPruning,
DeltaPruning. Default OFF: Razoring, MoveCountPruning, HistoryPruning, ProbCut,
DecisionImpact, RefutationSearch. Turning off DecisionImpact and RefutationSearch
selects the conventional PVS baseline. All search memory is transient.

The GUI must adjudicate draw claims. The engine considers both current claims
and claims by an intended move, while preserving checkmate precedence.
`isready` remains responsive during a search; state changes join the old search.
Ponder and infinite searches with a completed depth limit wait for the appropriate
`ponderhit`/`stop` before emitting `bestmove`.

## Reproduce comparisons

```powershell
build-v2/Release/axiom2_openings.exe > tests/axiom2-openings.epd
node tools/v2-compare.mjs
node tools/v2-arena.mjs nodes 12 10000
node tools/v2-arena.mjs time 12 10+0.1
node tools/v2-arena.mjs ablation 12 10+0.1
```

Arena uses the repository's fastchess executable, paired colors and 12 fixed
manually chosen opening positions. These are test starts, not an engine book.
Each run saves its exact command, fastchess configuration, log and PGN.
The comparator covers 10k/50k/100k/500k/1M node limits; it is a computational
comparison, not an Elo test. Some positions terminate early with a found mate.

## Debug and memory checks

```powershell
cmake --build build-v2 --config Debug --target axiom2_core_tests axiom2_search_tests
ctest --test-dir build-v2 -C Debug -R axiom2 --output-on-failure
cmake -S . -B build-v2-asan -A x64 -DAXIOM_SANITIZE=ON
cmake --build build-v2-asan --config RelWithDebInfo --target axiom2_core_tests axiom2_search_tests
ctest --test-dir build-v2-asan -C RelWithDebInfo -R axiom2 --output-on-failure
```

Run ASan executables from the developer terminal so the MSVC ASan runtime DLL
is on PATH. Use RelWithDebInfo, since Debug `/RTC` is incompatible with ASan.
The GCC/Clang sanitizer option enables address and undefined-behavior checks;
this Windows validation did not run a Linux compiler or ThreadSanitizer.

Optional `-DFATHOM_DIR=...` compiles the direct bitboard Syzygy adapter. Table
files are supplied by the user. Search WDL cutoffs and automatic root decisions
are limited to fresh zeroing states; other root DTZ results are advisory because
tablebases do not encode repetition history. No tables are bundled.

Portable scalar sliding attacks are the default. `AXIOM_NATIVE_AVX2=ON` permits
compiler AVX2 generation on a compatible CPU; there is no hand-written PEXT or
AVX2 attack kernel in this version.
