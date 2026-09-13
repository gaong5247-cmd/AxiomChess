# Incomplete pilot, not the final balanced regression

This run was deliberately stopped after discovering that the initial generated corpus contained only White-to-move positions and was lexicographically ordered. Completed rows in positions.jsonl remain useful individual depth-18 reference observations; this is not a completed 1,000-position run. summary.json contains only completed observations. The incomplete last position is not included. No files were deleted.

The replacement corpus is tests/middlegame-balanced.fens: both sides to move, deterministic shuffle, seed 20260913. Synthetic static-guided play remains unrepresentative of human or engine tournament games. Do not interpret either corpus as an estimate of real-game accuracy or Elo.
