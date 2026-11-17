#requires -Version 5.1
<#
.SYNOPSIS
    Fuehrt die Unit-Tests aus.

.EXAMPLE
    .\tools\test.ps1 -Config Debug
#>
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'Debug'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if (-not $env:BFKA_BUILD_ROOT) {
    . (Join-Path $PSScriptRoot 'dev-env.ps1') | Out-Null
}

$buildDir = Join-Path $env:BFKA_BUILD_ROOT $Config

$prev = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $env:BFKA_CMAKE '--build' $buildDir '--target' 'BFKA_Tests' 2>&1 | ForEach-Object { Write-Host $_ }
$code = $LASTEXITCODE
$ErrorActionPreference = $prev
if ($code -ne 0) { throw "Test-Build fehlgeschlagen (Exitcode $code)" }

$exe = Get-ChildItem $buildDir -Recurse -Filter 'BFKA_Tests.exe' -ErrorAction SilentlyContinue |
       Select-Object -First 1

if (-not $exe) { throw 'BFKA_Tests.exe nicht gefunden' }

Write-Host "Fuehre $($exe.FullName) aus" -ForegroundColor Cyan
& $exe.FullName
exit $LASTEXITCODE
