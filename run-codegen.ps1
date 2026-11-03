param([switch]$Diagnostic)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $cli = Join-Path $PSScriptRoot 'tools/rexglue-install/bin/rexglue.exe'
    if (-not (Test-Path -LiteralPath $cli)) { throw 'Run build-sdk.ps1 first.' }
    if (-not (Test-Path -LiteralPath 'assets/default.xex')) { throw 'Place your supported Disc 1 executable at assets/default.xex first.' }
    if ($Diagnostic) {
        & $cli --force --log-level debug codegen infinite_undiscovery_manifest.toml --ignore-stamp *> codegen-diagnostic-rerun.log
    } else {
        & $cli codegen infinite_undiscovery_manifest.toml --ignore-stamp *> codegen-validation-rerun.log
    }
    $result = $LASTEXITCODE
} finally { Pop-Location }
exit $result
