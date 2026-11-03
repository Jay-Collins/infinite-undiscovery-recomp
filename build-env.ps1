# Dot-source this file to prepare the current PowerShell for Windows x64 builds.
$ErrorActionPreference = 'Stop'
$iuVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $iuVsWhere)) { throw 'Install Visual Studio 2022 Build Tools with the C++ workload first.' }
$iuVsRoot = & $iuVsWhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $iuVsRoot) { throw 'Visual Studio 2022 C++ tools were not found.' }
$iuDevCmd = Join-Path $iuVsRoot 'Common7/Tools/VsDevCmd.bat'
New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot 'tools') | Out-Null
$iuEnvCmd = Join-Path $PSScriptRoot 'tools/load-msvc-env.cmd'
@("@call `"$iuDevCmd`" -no_logo -arch=x64 -host_arch=x64 >nul", '@if errorlevel 1 exit /b 1', '@set') |
    Set-Content -LiteralPath $iuEnvCmd -Encoding ASCII
$iuEnvLines = & $env:ComSpec /d /c $iuEnvCmd
if ($LASTEXITCODE -ne 0) { throw 'VsDevCmd failed.' }
foreach ($iuLine in $iuEnvLines) {
    if ($iuLine -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}
$iuToolDirs = @(
    (Join-Path $env:ProgramFiles 'LLVM/bin'),
    (Join-Path $iuVsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin'),
    (Join-Path $iuVsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja')
)
$env:PATH = ($iuToolDirs -join ';') + ';' + $env:PATH
foreach ($iuTool in 'clang', 'clang++', 'cmake', 'ninja', 'rc', 'link', 'git') {
    if (-not (Get-Command $iuTool -ErrorAction SilentlyContinue)) { throw "Missing build tool: $iuTool" }
}
