; Speedy MIDI installation script
; last modified: Holger Hoffmann 2013-09-14

#define MyAppName "Speedy MIDI"
#define MyAppVersion "1.1"
#define MyAppPublisher "Holger Hoffmann"
#define MyAppExeName "speedymidi.exe"

; Set these paths correctly on your system
#define QtBinPath "C:\Program Files\2010.05\qt\bin"
#define ReleasePath "..\shadowbuild\speedymidi\release"
#define OutputPath ".."

[Setup]
AppId={{7AFDF488-4BD9-45E5-9045-D1E91505F9A0}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL=http://speedymidi.sourceforge.net
AppSupportURL=http://sourceforge.net/projects/speedymidi/support
DefaultDirName={pf}\SpeedyMidi
DefaultGroupName={#MyAppName}
LicenseFile=..\LICENSE.GPL3
OutputDir={#OutputPath}
OutputBaseFilename=SpeedyMidiSetup-{#MyAppVersion}
SetupIconFile=res\installer_icon.ico
Compression=lzma
SolidCompression=yes
AppMutex=SpeedyMidiAntiUninstall
WizardImageFile=res\speedymidi-icon256.bmp
WizardSmallImageFile=res\speedymidi-icon48.bmp
WizardImageStretch=no
WizardImageBackColor=$402020
UninstallDisplayIcon={app}\bin\{#MyAppExeName}

[Languages]
Name: en; MessagesFile: compiler:Default.isl
Name: de; MessagesFile: compiler:Languages\German.isl

[CustomMessages]
en.ComponentDescription_Examples=Example MIDI Files
en.ExampleSubDir=Examples
en.Introduction=Introduction
en.ShowIntroduction=Show short introduction

en.FullInstallation=Full installation
en.CompactInstallation=Compact installation
en.CustomInstallation=Custom installation

de.ComponentDescription_Examples=MIDI-Beispieldateien
de.ExampleSubDir=Beispiele
de.Introduction=Einführung
de.ShowIntroduction=Kurze Einführung anzeigen

de.FullInstallation=Vollständige Installation
de.CompactInstallation=Kompakte Installation
de.CustomInstallation=Benutzerdefinierte Installation

[Tasks]
Name: desktopicon; Description: {cm:CreateDesktopIcon}

[Types]
Name: full; Description: {cm:FullInstallation}
Name: compact; Description: {cm:CompactInstallation}
Name: custom; Description: {cm:CustomInstallation}; Flags: iscustom

[Components]
Name: SpeedyMidi; Description: Speedy MIDI; Flags: fixed; Types: custom compact full
Name: Examples; Description: {cm:ComponentDescription_Examples}; Types: custom full

[Files]
Source: ..\README; DestName: README; DestDir: {app}; Components: SpeedyMidi; Flags: ignoreversion
Source: ..\LICENSE.GPL3; DestDir: {app}; Components: SpeedyMidi; Flags: ignoreversion
Source: ..\CHANGELOG; DestDir: {app}; Components: SpeedyMidi; Flags: ignoreversion
Source: {#ReleasePath}\speedymidi.exe; DestDir: {app}\bin; Components: SpeedyMidi; Flags: ignoreversion
Source: {#ReleasePath}\translations\*.*; DestDir: {app}\bin\translations; Components: SpeedyMidi; Flags: ignoreversion
Source: {#QtBinPath}\QtCore4.dll; DestDir: {app}\bin; Components: SpeedyMidi; Flags: ignoreversion
Source: {#QtBinPath}\QtGui4.dll; DestDir: {app}\bin; Components: SpeedyMidi; Flags: ignoreversion
Source: {#QtBinPath}\QtNetwork4.dll; DestDir: {app}\bin; Components: SpeedyMidi; Flags: ignoreversion
Source: {#QtBinPath}\QtXml4.dll; DestDir: {app}\bin; Components: SpeedyMidi; Flags: ignoreversion
Source: {#QtBinPath}\mingwm10.dll; DestDir: {app}\bin; Components: SpeedyMidi; Flags: ignoreversion
Source: {#QtBinPath}\libgcc_s_dw2-1.dll; DestDir: {app}\bin; Components: SpeedyMidi; Flags: ignoreversion
Source: res\introduction.en.txt; DestName: introduction.txt; DestDir: {app}; Components: SpeedyMidi; Flags: ignoreversion; Languages: en
Source: res\introduction.de.txt; DestName: introduction.txt; DestDir: {app}; Components: SpeedyMidi; Flags: ignoreversion; Languages: de
Source: examples\*.*; DestDir: {userdocs}\SpeedyMidi\{cm:ExampleSubDir}; Components: Examples; Flags: ignoreversion

[Dirs]
Name: {userdocs}\SpeedyMidi

[Icons]
;Program group
Name: {group}\{#MyAppName}; Filename: {app}\bin\{#MyAppExeName}; WorkingDir: {userdocs}\SpeedyMidi
Name: {group}\{cm:UninstallProgram,{#MyAppName}}; Filename: {uninstallexe}; IconFilename: {sys}\shell32.dll; IconIndex: 32
Name: {group}\{cm:Introduction}; Filename: {app}\introduction.txt

;Desktop icon
Name: {commondesktop}\{#MyAppName}; Filename: {app}\bin\{#MyAppExeName}; Tasks: desktopicon; WorkingDir: {userdocs}\SpeedyMidi

[Run]
Filename: {app}\bin\{#MyAppExeName}; Description: {cm:LaunchProgram,{#MyAppName}}; WorkingDir: {userdocs}\SpeedyMidi; Flags: nowait postinstall skipifsilent
Filename: {app}\introduction.txt; Description: {cm:ShowIntroduction}; Flags: postinstall shellexec skipifsilent

[Registry]
Root: HKCU; Subkey: Software\HoHo; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: Software\HoHo\SpeedyMIDI; Flags: uninsdeletekey
