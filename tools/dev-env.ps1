#requires -Version 5.1
<#
.SYNOPSIS
    Richtet die Build-Umgebung fuer Bass Forge Kick Architect ein.

.DESCRIPTION
    Sucht Visual Studio Build Tools (oder VS), CMake und Ninja und stellt sie in
    die aktuelle Sitzung. Dot-sourcing bindet die Umgebung an die aufrufende Shell:

        . .\tools\dev-env.ps1

    Danach sind `cmake`, `ninja` und `cl` verfuegbar.
#>

$ErrorActionPreference = 'Stop'

function Find-VsInstallation {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }

    $json = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json 2>$null
    if (-not $json) { return $null }

    $parsed = $json | ConvertFrom-Json
    if (-not $parsed) { return $null }
    return $parsed[0].installationPath
}

$vsPath = Find-VsInstallation
if (-not $vsPath) {
    throw 'Keine Visual-Studio-Installation mit C++-Werkzeugen gefunden. Bitte "Desktop development with C++" installieren.'
}

$cmake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'

foreach ($tool in @($cmake, $ninja, $vcvars)) {
    if (-not (Test-Path $tool)) { throw "Fehlendes Werkzeug: $tool" }
}

$env:BFKA_VS_PATH = $vsPath
$env:BFKA_CMAKE   = $cmake
$env:BFKA_NINJA   = $ninja
$env:BFKA_VCVARS  = $vcvars

$env:PATH = (Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin') + ';' +
            (Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja') + ';' +
            $env:PATH

# MSVC-Umgebung (cl.exe, link.exe) in die aktuelle Sitzung uebernehmen
$lines = & cmd.exe /c "`"$vcvars`" >nul 2>&1 && set" 2>$null
foreach ($line in $lines) {
    if ($line -match '^([^=]+)=(.*)$') {
        $name = $Matches[1]
        if ($name -notmatch '^(PROMPT|COUNTER)$') {
            Set-Item -Path ("Env:" + $name) -Value $Matches[2] -ErrorAction SilentlyContinue
        }
    }
}

Write-Host "VS:      $vsPath"
Write-Host "CMake:   $((& $cmake --version | Select-Object -First 1))"
Write-Host "Ninja:   $(& $ninja --version)"
Write-Host "cl:      $((cmd /c 'cl 2>&1' | Select-Object -First 1))"
Write-Host 'Umgebung bereit. Naechster Schritt: .\tools\configure.ps1' -ForegroundColor Green
