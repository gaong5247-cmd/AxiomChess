param(
    [string[]]$Features = @('correction'),
    [string[]]$ReferenceFeatures = @(),
    [ValidateRange(0,600000)][int]$Milliseconds = 0,
    [ValidateRange(1,16)][int]$Threads = 1,
    [ValidateRange(1,10000000)][int]$Nodes = 5000,
    [ValidateRange(1,30)][int]$Depth = 8,
    [string]$Engine = "$PSScriptRoot\..\build\Release\axiom-0.3-cpu.exe",
    [string]$Reference = '',
    [string]$Output = "$PSScriptRoot\..\results\bench.json"
)
$ErrorActionPreference = 'Stop'
if (-not $Reference) { $Reference = $Engine }
$rows = @()
$positions = Get-Content -LiteralPath "$PSScriptRoot\..\tests\bench.fens" | Where-Object { $_ -and -not $_.StartsWith('#') }
foreach ($fen in $positions) {
    foreach ($kind in @('reference','candidate')) {
        $exe = if ($kind -eq 'reference') { $Reference } else { $Engine }
        $engineArgs = @('--analyze','--fen',$fen,'--depth',"$Depth",'--nodes',"$Nodes",'--time',"$Milliseconds",'--proof-nodes','0')
        if ($kind -eq 'reference') { foreach ($feature in $ReferenceFeatures) { $engineArgs += @('--feature',$feature) } }
        if ($kind -eq 'candidate') {
            foreach ($feature in $Features) { $engineArgs += @('--feature',$feature) }
            $engineArgs += @('--threads',"$Threads")
        }
        $result = & $exe @engineArgs
        if ($LASTEXITCODE) { throw "$kind engine failed" }
        $parsed = ($result -join "`n") | ConvertFrom-Json
        $rows += [ordered]@{ configuration=$kind; fen=$fen; bestmove=$parsed.bestmove; depth=$parsed.depth; nodes=$parsed.nodes; elapsed_ms=$parsed.elapsed_ms; score=$parsed.score_cp; stats=$parsed.search_stats }
    }
}
$report = [ordered]@{
    kind='bounded_search_diagnostic_not_elo'; timestamp=(Get-Date).ToUniversalTime().ToString('o')
    features=$Features; reference_features=$ReferenceFeatures; milliseconds=$Milliseconds; threads=$Threads; node_limit=$Nodes; depth_limit=$Depth
    candidate_sha256=(Get-FileHash -LiteralPath $Engine).Hash
    reference_sha256=(Get-FileHash -LiteralPath $Reference).Hash
    observations=$rows
}
if (Test-Path -LiteralPath $Output) { throw "Refusing to overwrite existing report: $Output" }
$parent = Split-Path -Parent $Output
if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Output -Encoding utf8
Write-Output "Saved diagnostic benchmark: $Output"
