# External reference engine

The Axiom engine does not link Stockfish and does not require it at runtime. Stockfish is used only by the optional regression runner as an external UCI reference. Stockfish itself uses NNUE; Axiom uses its own handcrafted evaluation and integer search histories.

Downloaded official Stockfish 18 Windows x86-64 AVX2 release archive:

- Source: https://github.com/official-stockfish/Stockfish/releases/tag/sf_18
- Asset: stockfish-windows-x86-64-avx2.zip
- Archive bytes: 76,955,020
- Archive SHA-256: 6f6c272ebd6ea594377715235c8a7326f75940ef4f4f856f45106028fe6ae900
- Local executable: stockfish-18/stockfish/stockfish-windows-x86-64-avx2.exe

The archive digest was checked against the official GitHub release asset metadata before extraction; UCI identity was checked after extraction. Upstream license and source material from the archive are preserved in the extracted directory. Stockfish is GPLv3; consult its bundled license before redistributing. Keep the external-reference provenance separate from Axiom's independently implemented code.
