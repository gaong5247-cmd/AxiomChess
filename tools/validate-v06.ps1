$ErrorActionPreference='Stop'
Push-Location "$PSScriptRoot\.."
try {
    node tests/v06-output.mjs
    if($LASTEXITCODE) { throw 'v06 output validation failed' }
    node tests/audit-equivalence.mjs baseline/v05-final.exe build/Release/axiom-0.6-attribution.exe results/v06-final-equivalence.json
    if($LASTEXITCODE) { throw 'Default policy changed' }
    foreach($experiment in @(
        @('holdout','300','v06-correction-holdout-final'),
        @('safety','30','v06-safety-ablation-final'),
        @('attribution','10','v06-attribution-final'),
        @('ordering','30','v06-movepicker-final'),
        @('profile','30','v06-profile-final'),
        @('time','10','v06-time')
    )) {
        node tools/v06-experiments.mjs --mode $experiment[0] --limit $experiment[1] --out "results/$($experiment[2])"
        if($LASTEXITCODE) { throw "Experiment failed: $($experiment[0])" }
    }
} finally { Pop-Location }
