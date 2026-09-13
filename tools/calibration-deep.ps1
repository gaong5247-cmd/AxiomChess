param([string]$Engine="$PSScriptRoot\..\build\Release\axiom-0.5-calibration.exe",[string]$OutputDirectory="$PSScriptRoot\..\results\v05-deep")
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory) { throw 'Output directory exists' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$cases=@(
    @{name='singular-probcut';fen='r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1';flags=@('--experimental');depth=10;nodes=1000000},
    @{name='verified-null';fen='r3k2r/ppp2ppp/8/8/8/8/PPP2PPP/R2QK2R w KQkq - 0 1';flags=@('--feature','verified_null');depth=10;nodes=500000},
    @{name='iid';fen='rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';flags=@('--feature','iid');depth=7;nodes=500000}
)
foreach($case in $cases) {
    $engineArgs=@('--analyze','--fen',$case.fen,'--depth',"$($case.depth)",'--nodes',"$($case.nodes)",'--time','0','--proof-nodes','0')+$case.flags
    $data=& $Engine @engineArgs
    if($LASTEXITCODE) { throw "Search failed: $($case.name)" }
    $data | Set-Content -LiteralPath (Join-Path $OutputDirectory "$($case.name).json") -Encoding utf8
}
foreach($adaptive in @($false,$true)) {
    $engineArgs=@('--analyze','--middlegame','--feature','time_management','--time','250','--soft-time','70','--depth','30','--proof-nodes','0')
    if($adaptive) { $engineArgs+=@('--feature','adaptive_time') }
    $data=& $Engine @engineArgs
    if($LASTEXITCODE) { throw 'Time search failed' }
    $data | Set-Content -LiteralPath (Join-Path $OutputDirectory "time-$adaptive.json") -Encoding utf8
}
@{engine_sha256=(Get-FileHash -LiteralPath $Engine).Hash;cases=$cases;time_hard_ms=250;time_soft_ms=70;policy='Completed-call costs, not counterfactual node savings; finite exploratory fixtures'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'manifest.json') -Encoding utf8
'Saved deep feature costs and time-management observations'
