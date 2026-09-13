# Coverage correction

The 300 unique FENs are genuine played moves with separate unrestricted and
forced-played Stockfish depth-18 analyses. 270 have the same reference move and
low score variance over the final three completed iterations (depths 16–18).
This is not independent depth-18/20/22 confirmation.

The extractor stopped after the first qualifying observation per game/side and
used 10,000-node screening. Consequently it can miss earlier qualifying errors.
The three original FIRST_CRITICAL_MISTAKE / LARGEST_MISTAKE /
ALL_THRESHOLD_CROSSINGS exports contain the same selected sample. They are
**not valid exhaustive game-level selections** and must not be used as such.
They are retained to preserve provenance, not endorsed as completed outputs.

Use critical.jsonl as the observed critical corpus. Full-game first/largest/all
threshold extraction remains pending. All 300 selected positions are middlegames;
do not infer endgame or opening coverage. Original Axiom score/depth/PV are unknown.
