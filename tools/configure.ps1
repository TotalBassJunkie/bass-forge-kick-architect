#requires -Version 5.1
<#
.SYNOPSIS
    Konfiguriert den Build mit CMake + Ninja.

.EXAMPLE
    .\tools\configure.ps1 -Config Debug
    .\tools\configure.ps1 -Config Release -Clean
#>
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'Debug',

    [switch]$Clean,

    [string]$JucePath = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if (-not $env:BFKA_BUILD_ROOT) {
    . (Join-Path $PSScriptRoot 'dev-env.ps1') | Out-Null
}

$buildDir = Join-Path $env:BFKA_BUILD_ROOT $Config

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Loesche $buildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

$args = @(
    '-S', $root,
    '-B', $buildDir,
    '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$Config",
    "-DCMAKE_MAKE_PROGRAM=$env:BFKA_NINJA",
    '-DBFKA_BUILD_TESTS=ON'
)

if ($JucePath) { $args += "-DBFKA_JUCE_SOURCE_DIR=$JucePath" }

Write-Host "cmake $($args -join ' ')" -ForegroundColor Cyan

# CMake schreibt Fortschritt/Warnungen nach stderr; PowerShell wuerde das bei
# $ErrorActionPreference='Stop' als Abbruch werten. Exitcode ist das Kriterium.
$prev = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $env:BFKA_CMAKE @args 2>&1 | ForEach-Object { Write-Host $_ }
$code = $LASTEXITCODE
$ErrorActionPreference = $prev

if ($code -ne 0) { throw "Konfiguration fehlgeschlagen (Exitcode $code)" }

Write-Host "Konfiguriert in $buildDir" -ForegroundColor Green
