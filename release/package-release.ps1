# Package a previously built Windows Release target using a strict file allowlist.
param(
    [Parameter(Mandatory)][string]$BuildDir,
    [string]$Version = 'v1.0.0-rc2',
    [string]$OutputRoot = '',
    [string]$SdkProvenance = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ($Version -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+(?:-rc[0-9]+)?$') { throw 'Invalid release version' }
$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
if (-not $OutputRoot) { $OutputRoot = Join-Path $repo 'out/releases' }
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
if (-not $SdkProvenance) { $SdkProvenance = Join-Path $repo 'tools/rexglue-install/local-build.json' }
$cache = Get-Content -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt') -Raw
if ($cache -notmatch '(?m)^CMAKE_BUILD_TYPE:STRING=Release\r?$' -or
    $cache -notmatch '(?m)^IU_ENABLE_DIAGNOSTICS:BOOL=OFF\r?$') { throw 'A Release build with IU_ENABLE_DIAGNOSTICS=OFF is required' }
& cmake "-DVERIFY_DIR=$BuildDir" '-DREQUIRE_ALL=ON' "-DEXPECTED_HASH_FILE=$SdkProvenance" -P (Join-Path $repo 'cmake/verify_rexglue_dlls.cmake')
if ($LASTEXITCODE -ne 0) { throw 'SDK hash verification failed' }
$exe = Join-Path $BuildDir 'InfiniteUndiscoveryRecomp.exe'
$binaryText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($exe))
foreach ($marker in 'IU_ORB_MOVE_TRACE','IU_VESPLUME_TRACE_PATH','SAVE_ANYWHERE_REQUEST','bind_iu_community_debug','bind_iu_save_anywhere','bind_iu_safe_step','IU-PERF','IU_DIAG_TRACE_PATH') {
    if ($binaryText.Contains($marker)) { throw "Diagnostic code found in release executable: $marker" }
}
$stage = Join-Path $OutputRoot "InfiniteUndiscoveryRecomp-$Version-Windows-x64"
$zip = "$stage.zip"
if ((Test-Path -LiteralPath $stage) -or (Test-Path -LiteralPath $zip)) { throw 'Use a fresh release staging/output path' }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
foreach ($name in 'InfiniteUndiscoveryRecomp.exe','rexruntime.dll','rexgpu-xenos.dll') {
    Copy-Item -LiteralPath (Join-Path $BuildDir $name) -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $repo 'release/README.txt') -Destination $stage
foreach ($name in 'LICENSE','THIRD_PARTY_NOTICES.txt') { Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $stage }
Copy-Item -LiteralPath (Join-Path $repo 'LICENSES') -Destination $stage -Recurse
$revision = (& git -C $repo rev-parse HEAD).Trim()
@("Version=$Version", "Source=$revision", 'Configuration=Release', 'Diagnostics=OFF', 'Platform=Windows-x64') | Set-Content (Join-Path $stage 'BUILD_INFO.txt') -Encoding ASCII
$hashes = foreach ($file in Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName) {
    $relative = [IO.Path]::GetRelativePath($stage,$file.FullName).Replace('\','/')
    '{0}  {1}' -f (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash,$relative
}
$hashes | Set-Content (Join-Path $stage 'RELEASE_HASHES_SHA256.txt') -Encoding ASCII
# Never copy a runtime directory, profile, save, shader cache, test or game asset.
$allowedRoot = @('InfiniteUndiscoveryRecomp.exe','rexruntime.dll','rexgpu-xenos.dll','README.txt','LICENSE','THIRD_PARTY_NOTICES.txt','BUILD_INFO.txt','RELEASE_HASHES_SHA256.txt')
foreach ($item in Get-ChildItem -LiteralPath $stage -Force) {
    if ($item.PSIsContainer) { if ($item.Name -ne 'LICENSES') { throw "Unexpected release directory: $($item.Name)" } }
    elseif ($item.Name -notin $allowedRoot) { throw "Unexpected release file: $($item.Name)" }
}
foreach ($file in Get-ChildItem (Join-Path $stage 'LICENSES') -Recurse -File) {
    if ($file.Extension -notin '.txt','.md','.h','.LIB') { throw "Unexpected license asset: $($file.Name)" }
}
$defender = Join-Path $env:ProgramFiles 'Windows Defender/MpCmdRun.exe'
if (-not (Test-Path -LiteralPath $defender)) { throw 'Defender CLI is required for release scanning' }
& $defender -Scan -ScanType 3 -File $stage
if ($LASTEXITCODE -ne 0) { throw 'Staged release scan failed' }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
& $defender -Scan -ScanType 3 -File $zip
if ($LASTEXITCODE -ne 0) { throw 'ZIP scan failed' }
Get-FileHash -LiteralPath $zip -Algorithm SHA256
