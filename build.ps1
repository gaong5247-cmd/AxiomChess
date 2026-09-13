param(
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [string]$VisualStudio = 'C:\Program Files\Microsoft Visual Studio\18\Community',
    [switch]$NativeAVX2
)
$ErrorActionPreference = 'Stop'
# Normalize inherited duplicate Path/PATH entries in this process only.
$taskPath = $env:PATH
Remove-Item Env:Path -ErrorAction SilentlyContinue
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = $taskPath
Push-Location $PSScriptRoot
try {
    $avx = if ($NativeAVX2) { 'ON' } else { 'OFF' }
    cmake --preset vs2026 "-DCMAKE_GENERATOR_INSTANCE=$VisualStudio" "-DAXIOM_NATIVE_AVX2=$avx"
    if ($LASTEXITCODE) { throw 'CMake configuration failed' }
    cmake --build build --config $Configuration --parallel
    if ($LASTEXITCODE) { throw 'C++ build failed' }
    ctest --test-dir build -C $Configuration --output-on-failure
    if ($LASTEXITCODE) { throw 'Engine tests failed' }
    & "$PSScriptRoot\tests\uci-smoke.ps1" -Engine "$PSScriptRoot\build\$Configuration\axiom-1.0.exe"
} finally { Pop-Location }
