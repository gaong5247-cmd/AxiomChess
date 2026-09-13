param([string]$Engine="$PSScriptRoot\..\build\Release\axiom-0.3-cpu.exe")
$ErrorActionPreference='Stop'
$tactical = & $Engine --analyze --experimental --fen 'r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1' --depth 10 --nodes 1000000 --time 0 --proof-nodes 0
if ($LASTEXITCODE) { throw 'Experimental search failed' }
$a=($tactical -join "`n") | ConvertFrom-Json
foreach ($field in @('correction_updates','continuation_updates','capture_updates','singular_tests','singular_extensions','probcut_attempts','probcut_cutoffs','lmr_reductions','safety_protected','pawn_cache_hits')) {
    if ($a.search_stats.$field -le 0) { throw "Feature not exercised: $field" }
}
$quiet = & $Engine --analyze --feature verified_null --fen 'r3k2r/ppp2ppp/8/8/8/8/PPP2PPP/R2QK2R w KQkq - 0 1' --depth 10 --nodes 500000 --time 0 --proof-nodes 0
if ($LASTEXITCODE) { throw 'Null verification search failed' }
$b=($quiet -join "`n") | ConvertFrom-Json
if ($b.search_stats.null_verifications -le 0) { throw 'Null verification was not exercised' }
if ($a.nodes -gt 1000000 -or $b.nodes -gt 500000) { throw 'Node cap exceeded' }
'PASS deep feature activation, singular/ProbCut/verified null, safety and shared limits (not an Elo test)'
