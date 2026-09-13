# Axiom 0.6 feature lifecycle

No feature is promoted by this release. Fixed-node agreement and CPU speed are not Elo.

| Feature / policy | Status | Default | Evidence needed for promotion |
| --- | --- | --- | --- |
| Correction, continuation, capture history, countermove | STABLE_BASELINE | Middlegame ON | Larger isolated paired tests for further changes |
| Dynamic LMR, strategic evaluation, king safety, pawn cache | STABLE_BASELINE | Middlegame ON | Preserve correctness; paired strength evidence |
| Search Safety legacy mask | STABLE_BASELINE | Middlegame ON | Reason-level ablation and paired tests before weakening |
| New continuation/counter/king protection bits | EXPERIMENTAL | OFF in legacy mask | Cost / accuracy / fixed-time ablation |
| Partial top-eight ordering | EXPERIMENTAL | OFF | Fixed-depth/nodes/time A/B, tactical regression, paired tests |
| TimeGuard next-iteration estimator | EXPERIMENTAL | OFF | UCI clock stress and no increased time forfeits, then paired tests |
| calibrated_lmr | REJECTED_CANDIDATE | OFF | Retune and repeat isolated tests |
| adaptive_time | REJECTED_CANDIDATE | OFF | Redesign; time-forfeit evidence required |
| rfp | EXPERIMENTAL_LOW_VALUE | OFF | Useful cutoff rate and strength evidence |
| singular, verified_null, probcut, history_pruning, see_pruning, mate_distance, iid, tt_policy, time_management, trend_safety | EXPERIMENTAL | OFF | Isolated correctness, efficiency, time and paired evidence |

`--experimental` explicitly enables all Features flags, including rejected candidates;
it is a stress/forensic configuration, not a recommended playing preset.
`--no-lmr` disables baseline and dynamic reductions; disabling `dynamic_lmr` alone does not.
`--no-null` disables actual null-move search; disabling `verified_null` alone does not.

Safety counters use joint reason credit for skipped eligible checks. They are not
counterfactual node savings, prevented blunders or calibrated probabilities.
RFP and singular retain independent correctness guards; their safety-matrix rows
are currently zero (not connected to the configurable mask). TT/PV/check guards
also remain outside the mask where required by each heuristic.
