param([ValidateRange(1,128)][int]$Jobs = 8)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'build-env.ps1')
$sdkSource = Join-Path $PSScriptRoot 'tools/rexglue-sdk'
$sdkInstall = Join-Path $PSScriptRoot 'tools/rexglue-install'
$sdkBuild = Join-Path $sdkSource 'out/build/win-amd64'
$pinnedCommit = 'f5337cdc947ff6d4c4196737e2c807a48f2a1fc2'
if ((& git -C $sdkSource rev-parse HEAD) -ne $pinnedCommit) { throw "SDK must be checked out at $pinnedCommit" }
$submodules = & git -C $sdkSource submodule status --recursive
if ($LASTEXITCODE -ne 0 -or ($submodules | Where-Object { $_ -match '^[-+U]' })) {
    throw 'Initialize all SDK submodules at their pinned revisions first.'
}

# Carry narrow, reviewed runtime fixes on top of the pinned base revision.
$patchFiles = @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'patches/rexglue') -Filter '*.patch' | Sort-Object Name)
foreach ($patch in $patchFiles) {
    & git -C $sdkSource apply --check $patch.FullName 2>$null
    if ($LASTEXITCODE -eq 0) {
        & git -C $sdkSource apply $patch.FullName
        if ($LASTEXITCODE -ne 0) { throw "Failed SDK patch: $($patch.Name)" }
    } else {
        & git -C $sdkSource apply --reverse --check $patch.FullName 2>$null
        if ($LASTEXITCODE -ne 0) { throw "SDK patch neither applicable nor already applied: $($patch.Name)" }
    }
}

# Git on Windows can materialize Unix symlinks as small text files. Replace
# only those libmspack placeholders with bytes from their checked-in targets.
$mspackRoot = Join-Path $sdkSource 'thirdparty/libmspack'
$mspackEntries = & git -C $mspackRoot ls-files -s
foreach ($entry in $mspackEntries) {
    if ($entry -notmatch '^120000 [0-9a-f]+ 0\t(.+)$') { continue }
    $relative = $Matches[1]
    $destination = Join-Path $mspackRoot $relative
    $item = Get-Item -LiteralPath $destination
    if ($item.LinkType -or $item.Length -gt 260) { continue }
    $linkText = [IO.File]::ReadAllText($destination).Trim()
    $target = [IO.Path]::GetFullPath((Join-Path (Split-Path $destination -Parent) $linkText))
    if (-not $target.StartsWith($mspackRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "libmspack link escapes its source tree: $relative"
    }
    if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw "Missing symlink target: $target" }
    Copy-Item -LiteralPath $target -Destination $destination
    Write-Host "Materialized libmspack symlink: $relative"
}

Push-Location $sdkSource
try {
    & cmake --preset win-amd64 "-DCMAKE_INSTALL_PREFIX=$sdkInstall" -DREXGLUE_USE_D3D12=ON -DREXGLUE_USE_VULKAN=OFF
    if ($LASTEXITCODE -ne 0) { throw 'SDK configuration failed.' }
    & cmake --build $sdkBuild --config Release --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'SDK build failed.' }
    & cmake --install $sdkBuild --config Release --prefix $sdkInstall
    if ($LASTEXITCODE -ne 0) { throw 'SDK installation failed.' }
} finally { Pop-Location }

$sourceDiff = Join-Path $sdkInstall 'local-source.diff'
& git -C $sdkSource diff --binary "--output=$sourceDiff"
if ($LASTEXITCODE -ne 0) { throw 'Could not record SDK source changes.' }
$provenance = [ordered]@{
    commit = $pinnedCommit
    configuration = 'Release'
    clang = (& clang --version | Select-Object -First 1)
    cmake = (& cmake --version | Select-Object -First 1)
    ninja = (& ninja --version)
    msvc = $env:VCToolsVersion
    windowsSdk = $env:WindowsSDKVersion
    submodules = @($submodules)
    patches = @($patchFiles | ForEach-Object { [ordered]@{ name=$_.Name; sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash } })
    sourceDiffSha256 = (Get-FileHash -LiteralPath $sourceDiff -Algorithm SHA256).Hash
    hashes = [ordered]@{}
}
foreach ($dll in 'rexruntime.dll', 'rexgpu-xenos.dll') {
    $provenance.hashes[$dll] = (Get-FileHash -LiteralPath (Join-Path $sdkInstall "bin/$dll") -Algorithm SHA256).Hash
}
$provenance | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $sdkInstall 'local-build.json') -Encoding UTF8
Write-Host "Installed pinned SDK: $sdkInstall"
