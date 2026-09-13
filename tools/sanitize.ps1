param([string]$VisualStudio='C:\Program Files\Microsoft Visual Studio\18\Community')
$ErrorActionPreference='Stop'
$taskPath=$env:PATH
Remove-Item Env:Path -ErrorAction SilentlyContinue
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path=$taskPath
Push-Location "$PSScriptRoot\.."
try {
    cmake -S . -B build-asan -G 'Visual Studio 18 2026' -A x64 "-DCMAKE_GENERATOR_INSTANCE=$VisualStudio" -DAXIOM_SANITIZE=ON
    if($LASTEXITCODE) { throw 'ASan configuration failed' }
    cmake --build build-asan --config RelWithDebInfo --parallel
    if($LASTEXITCODE) { throw 'ASan build failed' }
    $taskVersion=(Get-Content -LiteralPath "$VisualStudio\VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt").Trim()
    $taskRuntime="$VisualStudio\VC\Tools\MSVC\$taskVersion\bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll"
    if(-not (Test-Path -LiteralPath $taskRuntime)) { throw 'MSVC AddressSanitizer runtime missing' }
    Copy-Item -LiteralPath $taskRuntime -Destination 'build-asan\RelWithDebInfo\clang_rt.asan_dynamic-x86_64.dll'
    ctest --test-dir build-asan -C RelWithDebInfo --output-on-failure
    if($LASTEXITCODE) { throw 'ASan tests failed' }
} finally { Pop-Location }
