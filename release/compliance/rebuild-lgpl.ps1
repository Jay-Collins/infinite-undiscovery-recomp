param([ValidateRange(1,128)][int]$Jobs=8)
$ErrorActionPreference='Stop'
# Run from an x64 Visual Studio developer PowerShell with Clang/CMake/Ninja.
$source=Join-Path $PSScriptRoot 'sdk-source'
$build=Join-Path $PSScriptRoot 'rebuild'
& cmake -S $source -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DREXGLUE_USE_D3D12=ON -DREXGLUE_USE_VULKAN=OFF
if ($LASTEXITCODE -ne 0) { throw 'LGPL source configure failed' }
& cmake --build $build --target mspack libavutil libavcodec --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'LGPL library rebuild failed' }
foreach ($name in 'mspack.lib','libavcodec.lib','libavutil.lib') {
    Copy-Item -LiteralPath (Join-Path $source "out/win-amd64/$name") -Destination (Join-Path $PSScriptRoot "libraries/$name")
}
& (Join-Path $PSScriptRoot 'relink.ps1')
