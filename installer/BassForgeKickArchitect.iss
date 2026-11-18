; B-134 - Inno-Setup-Skript fuer Bass Forge Kick Architect (Windows x64)
;
; Aufruf:    iscc installer\BassForgeKickArchitect.iss
; Ergebnis:  ..\release\BassForgeKickArchitect-Setup-<Version>.exe
; Vorher:    .\tools\build.ps1 -Config Release
;
; Version wird hier VON HAND gepflegt: das Skript liest CMakeLists.txt NICHT.
; Bei einer neuen Projektversion beide Stellen anpassen (project(... VERSION ...)
; in CMakeLists.txt und MyAppVersion hier).
;
; Registry: es wird nichts geschrieben ausser den Eintraegen, die Inno fuer den
; Deinstaller selbst braucht. Das VST3-Ziel ist standardmaessig
; {commoncf64}\VST3 und laesst sich im Assistenten auf einen eigenen Host-Ordner
; umbiegen (siehe [Code] unten).

#define MyAppName "Bass Forge Kick Architect"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Bass Forge"
#define MyAppExeName "Bass Forge Kick Architect.exe"

; Release-Baum. Das Ticket nennt den Pfad relativ zum Repo-Wurzelverzeichnis
; ("..\..\bfka-build"); Inno loest Source-Pfade aber relativ zu dieser Datei
; (installer\) auf. Deshalb steht hier der absolute Standard aus
; tools/dev-env.ps1 / tools/build.ps1. Bei abweichendem BFKA_BUILD_ROOT nur
; diese eine Zeile aendern.
#define BuildRoot "C:\bfka-build"
#define Artefacts BuildRoot + "\Release\BassForgeKickArchitect_artefacts\Release"

[Setup]
AppId={{8F2A1C64-5A3D-4B7E-9C21-7B4E5D3A9F10}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Bass Forge\Kick Architect
DisableProgramGroupPage=yes
DefaultGroupName={#MyAppName}
OutputDir=..\release
OutputBaseFilename=BassForgeKickArchitect-Setup-{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\{#MyAppExeName}

[Tasks]
Name: "desktopicon"; Description: "Desktop-Symbol anlegen"; GroupDescription: "Zusaetzliche Verknuepfungen:"; Flags: unchecked

[Files]
; Standalone-Programm in den Programmordner.
Source: "{#Artefacts}\Standalone\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
; VST3-Bundle (Ordner .vst3 mit Contents\x86_64-win) in den gewaehlten VST3-Ordner.
Source: "{#Artefacts}\VST3\Bass Forge Kick Architect.vst3\*"; DestDir: "{code:GetVst3Dir}\Bass Forge Kick Architect.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{#MyAppName} starten"; Flags: nowait postinstall skipifsilent

[Code]
var
  Vst3Page: TInputDirWizardPage;

procedure InitializeWizard;
begin
  Vst3Page := CreateInputDirPage (wpSelectDir,
    'VST3-Ordner',
    'Wohin soll das VST3 installiert werden?',
    'Standard ist der gemeinsame VST3-Ordner von Windows. Ein eigener ' +
    'Host-Ordner kann hier eingetragen werden.',
    False, '');
  Vst3Page.Add ('');
  Vst3Page.Values[0] := ExpandConstant ('{commoncf64}\VST3');
end;

function GetVst3Dir (Param: String): String;
begin
  Result := Vst3Page.Values[0];
  if Result = '' then
    Result := ExpandConstant ('{commoncf64}\VST3');
end;
