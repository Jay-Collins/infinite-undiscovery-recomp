# Build and package a fresh public release. Supply game files and build the SDK first.
param([ValidateRange(1,128)][int]$Jobs=8)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $repo 'build-env.ps1')
& (Join-Path $repo 'run-codegen.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Code generation failed' }
$sdk=Join-Path $repo 'tools/rexglue-install'
$provenance=Join-Path $sdk 'local-build.json'
$build=Join-Path $repo ('out/build/public-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
if (Test-Path -LiteralPath $build) { throw 'Release build directory must be new' }
& cmake -S $repo -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ "-DCMAKE_PREFIX_PATH=$sdk" "-Drexglue_DIR=$sdk/lib/cmake/rexglue" "-DIU_LOCAL_REXGLUE_HASH_FILE=$provenance" -DIU_ENABLE_DIAGNOSTICS=OFF -DIU_BUILD_SETUP_TESTS=OFF -DREXGLUE_RECOMP_DEBUG_INFO=none
if ($LASTEXITCODE -ne 0) { throw 'Release configure failed' }
& cmake --build $build --target infinite_undiscovery --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'Release build failed' }
& (Join-Path $PSScriptRoot 'package-release.ps1') -BuildDir $build -SdkProvenance $provenance
