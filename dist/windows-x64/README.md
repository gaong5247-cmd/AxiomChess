# AxiomChess 2.0 — Windows x64

Add `axiom-2.exe` as a UCI engine in your chess GUI. The default is one thread,
32 MB hash, 50 ms move overhead, standard chess. No model or book files are needed.
The Microsoft Visual C++ runtime may be required.

Built with MSVC 19.51.36257.0, Release x64, AVX2 option OFF.

SHA-256:
`ea1b3ae7de40f9bfb35d944bd7974312a0783553627231429ce4859cb0f4a63f`

See [the report](../../AXIOM_2_REPORT.md) for measured results and limitations,
and [build instructions](../../AXIOM_2_BUILD.md) for reproduction.
DecisionImpact and RefutationSearch are experimental and default OFF.
