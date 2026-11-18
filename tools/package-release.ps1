#requires -Version 5.1
# B-134 - Portables Release-ZIP (Standalone + VST3) und Pruefsummen.
<#
.SYNOPSIS
    Packt die Release-Artefakte als portables ZIP und schreibt SHA256SUMS.txt.

.DESCRIPTION
    Liest die Artefakte aus $env:BFKA_BUILD_ROOT (Default C:\bfka-build) und legt
    unter release\ an:

        BassForgeKickArchitect-<Version>-portable-win64.zip
        SHA256SUMS.txt

    Das ZIP enthaelt zwei Ordner:

        Standalone\Bass Forge Kick Architect.exe
        VST3\Bass Forge Kick Architect.vst3\...

    Es wird nichts geloescht und nichts ueberschrieben ausser den beiden
    Ausgabedateien dieses Skripts. release\ wird nur angelegt, nie geleert.
    Es wird KEIN git, KEIN cmake und KEIN Test gestartet.

.EXAMPLE
    .\tools\package-release.ps1 -Config Release -Version "0.1.0"
#>
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'Release',

    [string]$Version = '0.1.0'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot

if (-not $env:BFKA_BUILD_ROOT) {
    $env:BFKA_BUILD_ROOT = 'C:\bfka-build'
}

$buildRoot = $env:BFKA_BUILD_ROOT
$artefacts = Join-Path $buildRoot (Join-Path $Config 'BassForgeKickArchitect_artefacts')

$standaloneExe = Join-Path $artefacts "$Config\Standalone\Bass Forge Kick Architect.exe"
$vst3Bundle    = Join-Path $artefacts "$Config\VST3\Bass Forge Kick Architect.vst3"

Write-Host "Build-Wurzel: $buildRoot"
Write-Host "Artefakte:    $artefacts"

if (-not (Test-Path $artefacts)) {
    throw "Artefaktordner fehlt: $artefacts. Zuerst .\tools\build.ps1 -Config $Config"
}
if (-not (Test-Path $standaloneExe)) {
    throw "Standalone-EXE fehlt: $standaloneExe. Zuerst .\tools\build.ps1 -Config $Config -Target BassForgeKickArchitect_Standalone"
}
if (-not (Test-Path $vst3Bundle)) {
    throw "VST3-Bundle fehlt: $vst3Bundle. Zuerst .\tools\build.ps1 -Config $Config -Target BassForgeKickArchitect_VST3"
}

$releaseDir = Join-Path $repoRoot 'release'
$zipName    = "BassForgeKickArchitect-$Version-portable-win64.zip"
$zipPath    = Join-Path $releaseDir $zipName

# Eigenes Staging-Verzeichnis (wird am Ende wieder entfernt); nichts Fremdes anfassen.
$staging = Join-Path ([System.IO.Path]::GetTempPath()) ("bfka-package-" + [System.Guid]::NewGuid().ToString('N'))

try {
    if (-not (Test-Path $releaseDir)) {
        New-Item -ItemType Directory -Path $releaseDir | Out-Null
        Write-Host "Angelegt: $releaseDir"
    }

    $stageStandalone = Join-Path $staging 'Standalone'
    $stageVst3       = Join-Path $staging 'VST3'
    New-Item -ItemType Directory -Path $stageStandalone | Out-Null
    New-Item -ItemType Directory -Path $stageVst3 | Out-Null

    Write-Host "Kopiere Standalone ..."
    Copy-Item -Path $standaloneExe -Destination $stageStandalone -Force

    Write-Host "Kopiere VST3-Bundle ..."
    $vst3Copy = Join-Path $stageVst3 'Bass Forge Kick Architect.vst3'
    Copy-Item -Path $vst3Bundle -Destination $vst3Copy -Recurse -Force
    # Linker-/Compiler-Reste gehoeren nicht ins Paket (falls der Build sie ablegt).
    Get-ChildItem -Path $vst3Copy -Recurse -File -Include *.lib, *.exp, *.pdb -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue

    if (Test-Path $zipPath) {
        Write-Host "Ueberschreibe vorhandenes ZIP: $zipPath"
        Remove-Item -Path $zipPath -Force
    }

    Write-Host "Packe ZIP ..."
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::CreateFromDirectory($staging, $zipPath,
        [System.IO.Compression.CompressionLevel]::Optimal, $false)

    # Pruefsummen: ZIP plus die beiden Einzelartefakte, damit sich auch die
    # ungepackten Dateien pruefen lassen.
    $sumFile = Join-Path $releaseDir 'SHA256SUMS.txt'
    $lines = @()
    foreach ($entry in @($zipPath, $standaloneExe)) {
        $hash = (Get-FileHash -Path $entry -Algorithm SHA256).Hash.ToLowerInvariant()
        $lines += ("{0}  {1}" -f $hash, (Split-Path -Leaf $entry))
    }
    Set-Content -Path $sumFile -Value $lines -Encoding ASCII
    Write-Host "Pruefsummen: $sumFile"

    $size = (Get-Item $zipPath).Length
    Write-Host ''
    Write-Host 'Fertig:' -ForegroundColor Green
    Write-Host ("  " + $zipPath + "  (" + [math]::Round($size / 1MB, 2) + " MB)")
    Write-Host ("  " + $sumFile)
}
catch {
    throw ("Paketierung fehlgeschlagen: " + $_.Exception.Message)
}
finally {
    if (Test-Path $staging) {
        Remove-Item -Path $staging -Recurse -Force -ErrorAction SilentlyContinue
    }
}
