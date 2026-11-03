$ErrorActionPreference='Stop'
Push-Location $PSScriptRoot
try {
    & clang++ '@relink.rsp'
    if ($LASTEXITCODE -ne 0) { throw 'Runtime relink failed' }
    Write-Host 'Produced rexruntime.dll in this archive directory.'
} finally { Pop-Location }
