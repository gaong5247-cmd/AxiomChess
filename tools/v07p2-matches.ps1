$ErrorActionPreference='Stop'
$common=@{
 Fastchess='reference/fastchess-1.8.2/fastchess-windows-x86-64/fastchess.exe'
 Openings='results/v07p2-openings/openings.epd'
 BaseFeatures=@('correction','continuation','capture_history','countermove','dynamic_lmr','strategic_eval','king_safety','search_safety','pawn_cache')
 ReferenceEngine='baseline/v06-final.exe';Threads=1;Concurrency=2;Seed=2026091302
}
& "$PSScriptRoot/run-sprt.ps1" @common -Feature legal_fast_path -Engine build/Release/axiom-0.7-cost.exe -Pairs 10 -TimeControl '1+0.01' -OutputDirectory results/v07p2-smoke *> results/v07p2-smoke-launch.log
node tools/match-summary.mjs results/v07p2-smoke
$smoke=Get-Content results/v07p2-smoke/game-summary.json -Raw | ConvertFrom-Json
foreach($p in $smoke.players.PSObject.Properties.Value) {if($p.illegal_moves -or $p.time_forfeits){throw 'Smoke safety failure: do not advance'}}
& "$PSScriptRoot/run-sprt.ps1" @common -Feature legal_fast_path -Engine build/Release/axiom-0.7-cost.exe -Pairs 50 -TimeControl '1+0.01' -OutputDirectory results/v07p2-paired *> results/v07p2-paired-launch.log
node tools/match-summary.mjs results/v07p2-paired
& "$PSScriptRoot/run-sprt.ps1" @common -Feature legal_fast_path -Engine build/Release/axiom-0.7-cost.exe -Pairs 10 -TimeControl '5+0.05' -OutputDirectory results/v07p2-medium *> results/v07p2-medium-launch.log
node tools/match-summary.mjs results/v07p2-medium
& "$PSScriptRoot/run-sprt.ps1" @common -Feature reuse_move_facts -Engine baseline/v07-phase1.exe -Pairs 150 -TimeControl '1+0.01' -OutputDirectory results/v07p2-phase1-replication *> results/v07p2-replication-launch.log
node tools/match-summary.mjs results/v07p2-phase1-replication
