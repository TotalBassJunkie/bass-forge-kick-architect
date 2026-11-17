#requires -Version 5.1
<#
.SYNOPSIS
    Baut das Plugin (VST3 + Standalone) und die Tests.

.EXAMPLE
    .\tools\build.ps1 -Config Release
    .\tools\build.ps1 -Config Debug -Target BFKA_Tests
#>
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'Debug',

    [string]$Target = 'all',

    [int]$Jobs = 0
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if (-not $env:BFKA_BUILD_ROOT) {
    . (Join-Path $PSScriptRoot 'dev-env.ps1') | Out-Null
}

$buildDir = Join-Path $env:BFKA_BUILD_ROOT $Config

if (-not (Test-Path $buildDir)) {
    throw "Nicht konfiguriert: $buildDir. Zuerst .\tools\configure.ps1 -Config $Config"
}

if (-not $env:BFKA_CMAKE) {
    . (Join-Path $PSScriptRoot 'dev-env.ps1') | Out-Null
}

$args = @('--build', $buildDir, '--config', $Config)
if ($Target -ne 'all') { $args += @('--target', $Target) }
if ($Jobs -gt 0)        { $args += @('--parallel', $Jobs) }

Write-Host "cmake $($args -join ' ')" -ForegroundColor Cyan

$prev = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $env:BFKA_CMAKE @args 2>&1 | ForEach-Object { Write-Host $_ }
$code = $LASTEXITCODE
$ErrorActionPreference = $prev

if ($code -ne 0) { throw "Build fehlgeschlagen (Exitcode $code)" }

$artefacts = Join-Path $buildDir 'BassForgeKickArchitect_artefacts'
if (Test-Path $artefacts) {
    Write-Host ''
    Write-Host 'Artefakte:' -ForegroundColor Green
    Get-ChildItem $artefacts -Recurse -Include *.vst3, *.exe -ErrorAction SilentlyContinue |
        ForEach-Object { Write-Host ("  " + $_.FullName) }
}
