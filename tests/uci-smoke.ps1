param([Parameter(Mandatory=$true)][string]$Engine)
$ErrorActionPreference = 'Stop'
$start = [System.Diagnostics.ProcessStartInfo]::new()
$start.FileName = $Engine
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardInput = $true
$start.RedirectStandardOutput = $true
$process = [System.Diagnostics.Process]::Start($start)
function Send-Uci([string]$Command) { $process.StandardInput.WriteLine($Command); $process.StandardInput.Flush() }
function Read-Until([string]$Pattern) {
    $lines = [System.Collections.Generic.List[string]]::new()
    for ($i=0; $i -lt 200; $i++) {
        $pending = $process.StandardOutput.ReadLineAsync()
        if (-not $pending.Wait(10000)) { throw "UCI timeout waiting for $Pattern" }
        $line = $pending.Result
        if ($null -eq $line) { throw 'Engine exited unexpectedly' }
        $lines.Add($line)
        if ($line -match $Pattern) { return $lines.ToArray() }
    }
    throw "Too many lines waiting for $Pattern"
}
try {
    Send-Uci 'uci'
    $handshake = Read-Until '^uciok$'
    if (-not ($handshake -match '^id name Axiom')) { throw 'Missing engine identity' }
    Send-Uci 'isready'
    $null = Read-Until '^readyok$'
    Send-Uci 'setoption name ProofNodes value 0'
    Send-Uci 'position startpos moves e2e4 e7e5'
    Send-Uci 'go depth 2'
    $analysis = Read-Until '^bestmove [a-h][1-8][a-h][1-8][qrbn]?$'
    if (-not ($analysis -match '^info depth 2')) { throw 'Depth 2 not reached' }
    Send-Uci 'setoption name Experimental value true'
    Send-Uci 'setoption name Threads value 3'
    Send-Uci 'go infinite'
    Send-Uci 'isready'
    $null = Read-Until '^readyok$'
    Send-Uci 'stop'
    $null = Read-Until '^bestmove '
    Send-Uci 'position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1'
    Send-Uci 'go depth 2'
    $null = Read-Until '^bestmove 0000$'
    Send-Uci 'quit'
    if (-not $process.WaitForExit(5000) -or $process.ExitCode -ne 0) { throw 'UCI quit failed' }
    Write-Output 'PASS UCI handshake, finite search, concurrent isready, stop, stalemate, quit'
} finally {
    if (-not $process.HasExited) { $process.Kill() }
    $process.Dispose()
}
