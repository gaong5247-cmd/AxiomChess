param(
    [Parameter(Mandatory=$true)][string]$Fastchess,
    [Parameter(Mandatory=$true)][string]$Openings,
    [ValidateSet('correction','continuation','capture_history','countermove','singular','verified_null','probcut','dynamic_lmr','history_pruning','see_pruning','mate_distance','iid','tt_policy','time_management','strategic_eval','king_safety','search_safety','pawn_cache','lazy_smp','rfp','version','trend_safety','calibrated_lmr','adaptive_time')]
    [string]$Feature = 'correction',
    [string[]]$BaseFeatures = @(),
    [string]$Engine = "$PSScriptRoot\..\build\Release\axiom-0.3-cpu.exe",
    [string]$ReferenceEngine = '',
    [ValidateRange(1,16)][int]$Threads = 1,
    [ValidateRange(2,16)][int]$CandidateThreads = 2,
    [ValidateRange(1,64)][int]$Concurrency = 1,
    [ValidateRange(1,1000000)][int]$Pairs = 5000,
    [string]$TimeControl = '10+0.1',
    [int]$Seed = 20260911,
    [string]$OutputDirectory = '',
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
$valid = @('correction','continuation','capture_history','countermove','singular','verified_null','probcut','dynamic_lmr','history_pruning','see_pruning','mate_distance','iid','tt_policy','time_management','strategic_eval','king_safety','search_safety','pawn_cache')
foreach ($base in $BaseFeatures) { if ($base -notin $valid -or $base -eq $Feature) { throw "Invalid or already-enabled base feature: $base" } }
$enginePath = (Resolve-Path -LiteralPath $Engine).Path
$referencePath = if ($ReferenceEngine) { (Resolve-Path -LiteralPath $ReferenceEngine).Path } else { $enginePath }
if ($Feature -eq 'version' -and -not $ReferenceEngine) { throw 'Version comparison requires ReferenceEngine' }
$openingPath = (Resolve-Path -LiteralPath $Openings).Path
if (-not $DryRun) { $Fastchess = (Resolve-Path -LiteralPath $Fastchess).Path }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path "$PSScriptRoot\..\results" ("sprt-$Feature-" + [guid]::NewGuid().ToString('N')) }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw "Refusing to reuse existing result directory: $OutputDirectory" }
$candidate = Join-Path $OutputDirectory 'candidate.exe'
$reference = Join-Path $OutputDirectory 'reference.exe'
$candidateThreadCount = if ($Feature -eq 'lazy_smp') { $CandidateThreads } else { $Threads }
$runnerArgs = @('-engine',"cmd=$candidate","name=candidate_$Feature",'option.Experimental=false',"option.Threads=$candidateThreadCount")
foreach ($base in $BaseFeatures) { $runnerArgs += "option.Feature_$base=true" }
if ($Feature -notin @('lazy_smp','version')) { $runnerArgs += "option.Feature_$Feature=true" }
$runnerArgs += @('-engine',"cmd=$reference",'name=reference','option.Experimental=false',"option.Threads=$Threads")
foreach ($base in $BaseFeatures) { $runnerArgs += "option.Feature_$base=true" }
$runnerArgs += @('-each',"tc=$TimeControl",'proto=uci','option.Hash=32','option.ProofNodes=0',
    '-openings',"file=$openingPath",'format=epd','order=random','-srand',"$Seed",'-rounds',"$Pairs",'-games','2',
    '-concurrency',"$Concurrency",'-sprt','elo0=0','elo1=5','alpha=0.05','beta=0.05','model=logistic',
    '-pgnout',"file=$OutputDirectory\games.pgn",'append=false','nodes=true','nps=true','timeleft=true',
    '-log',"file=$OutputDirectory\runner.log",'append=false','level=info','-output','format=cutechess')
$manifest = [ordered]@{
    status=$(if ($DryRun) {'dry_run_not_executed'} else {'scheduled'})
    feature=$Feature; base_features=$BaseFeatures; reference_threads=$Threads; candidate_threads=$candidateThreadCount; concurrency=$Concurrency
    engine_sha256=(Get-FileHash -LiteralPath $enginePath).Hash
    reference_sha256=(Get-FileHash -LiteralPath $referencePath).Hash
    openings_sha256=(Get-FileHash -LiteralPath $openingPath).Hash
    h0_elo=0; h1_elo=5; alpha=0.05; beta=0.05; model='logistic'; max_games=2*$Pairs
    runner=$Fastchess; arguments=$runnerArgs; timestamp=(Get-Date).ToUniversalTime().ToString('o')
    policy='Review SPRT and failures. No automatic merge, feature promotion, or overwrite.'
}
if ($DryRun) { $manifest | ConvertTo-Json -Depth 5; return }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
# Freeze both binaries so later builds cannot change the ongoing experiment.
Copy-Item -LiteralPath $enginePath -Destination $candidate
Copy-Item -LiteralPath $referencePath -Destination $reference
$manifest.status='running'
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$OutputDirectory\manifest.json" -Encoding utf8
try {
    & $Fastchess @runnerArgs 2>&1 | Tee-Object -FilePath "$OutputDirectory\console.log"
    $exitCode = $LASTEXITCODE
    $manifest.status = if ($exitCode -eq 0) { 'finished_review_sprt_log' } else { 'runner_failed' }
    $manifest['exit_code'] = $exitCode
} finally {
    if ($manifest.status -eq 'running') { $manifest.status='interrupted_no_decision' }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$OutputDirectory\manifest.json" -Encoding utf8
}
if ($exitCode) { throw "Fastchess failed with exit code $exitCode" }
