param([ValidateSet('Release','Debug')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$taskPath=$env:PATH
Remove-Item Env:Path -ErrorAction SilentlyContinue
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path=$taskPath
Push-Location "$PSScriptRoot\.."
try {
    cmake -S . -B build-causal -G 'Visual Studio 18 2026' -A x64 '-DCMAKE_GENERATOR_INSTANCE=C:\Program Files\Microsoft Visual Studio\18\Community' -DAXIOM_NATIVE_AVX2=ON -DAXIOM_RESEARCH_TRACE=ON
    if($LASTEXITCODE) { throw 'Research configure failed' }
    cmake --build build-causal --config $Configuration --parallel
    if($LASTEXITCODE) { throw 'Research build failed' }
    ctest --test-dir build-causal -C $Configuration --output-on-failure
    if($LASTEXITCODE) { throw 'Research tests failed' }
    & tests/uci-smoke.ps1 -Engine "$PWD\build-causal\$Configuration\axiom-0.6-causal-research.exe"
} finally { Pop-Location }
