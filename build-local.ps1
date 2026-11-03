param(
    [switch]$SetupTestsOnly,
    [ValidateRange(1,128)][int]$Jobs = 8
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'build-env.ps1')
$sdkInstall = Join-Path $PSScriptRoot 'tools/rexglue-install'
$provenance = Join-Path $sdkInstall 'local-build.json'
$cli = Join-Path $sdkInstall 'bin/rexglue.exe'
$manifest = Join-Path $PSScriptRoot 'infinite_undiscovery_manifest.toml'
$gameXex = Join-Path $PSScriptRoot 'assets/default.xex'
$buildDir = Join-Path $PSScriptRoot 'out/build/local-release'
if (-not (Test-Path -LiteralPath $provenance)) { throw 'Run build-sdk.ps1 first.' }
& cmake "-DVERIFY_DIR=$sdkInstall/bin" '-DREQUIRE_ALL=ON' "-DEXPECTED_HASH_FILE=$provenance" -P (Join-Path $PSScriptRoot 'cmake/verify_rexglue_dlls.cmake')
if ($LASTEXITCODE -ne 0) { throw 'Local SDK hash verification failed.' }

if (-not $SetupTestsOnly) {
    if (-not (Test-Path -LiteralPath $gameXex)) {
        throw "Game executable required: $gameXex. Place your supported Disc 1 default.xex there, then rerun this script."
    }
    & (Join-Path $PSScriptRoot 'run-codegen.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Game code generation failed; see codegen-validation-rerun.log.' }
} elseif (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'generated/rexglue.cmake'))) {
    # The pinned CLI refreshes CMake integration before checking the XEX.
    # An absent XEX is expected here; no placeholder game code is created.
    Push-Location $PSScriptRoot
    try { & $cli codegen $manifest *> (Join-Path $PSScriptRoot 'tools/setup-codegen.log') }
    finally { Pop-Location }
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'generated/rexglue.cmake'))) {
        throw 'SDK did not generate CMake integration; see tools/setup-codegen.log.'
    }
}

& cmake -S $PSScriptRoot -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ "-DCMAKE_PREFIX_PATH=$sdkInstall" "-Drexglue_DIR=$sdkInstall/lib/cmake/rexglue" "-DIU_LOCAL_REXGLUE_HASH_FILE=$provenance" -DIU_BUILD_SETUP_TESTS=ON -DIU_ENABLE_DIAGNOSTICS=ON
if ($LASTEXITCODE -ne 0) { throw 'Game project configuration failed.' }
$target = if ($SetupTestsOnly) { 'iu_asset_setup_tests' } else { 'infinite_undiscovery' }
& cmake --build $buildDir --target $target --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'Project build failed.' }
if ($SetupTestsOnly) {
    # This standalone test target does not use rexglue_setup_target's DLL staging.
    Copy-Item -LiteralPath (Join-Path $sdkInstall 'bin/rexruntime.dll') -Destination $buildDir
    $testOutput = Join-Path $PSScriptRoot ('out/tests/profiles-' + [Guid]::NewGuid().ToString('N'))
    & (Join-Path $buildDir 'iu_asset_setup_tests.exe') profile-tests $testOutput
    if ($LASTEXITCODE -ne 0) { throw 'Profile tests failed.' }
} else {
    Write-Host "Ready to run: $buildDir/InfiniteUndiscoveryRecomp.exe"
}
