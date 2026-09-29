#ifndef PackageDir
  #error PackageDir is required
#endif
#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif

[Setup]
AppId={{C073973C-14BC-4AF9-907A-BF9E47D3620E}
AppName=Speed MIDI Editor
AppVersion={#AppVersion}
AppPublisher=Speed MIDI Editor contributors
AppPublisherURL=https://github.com/RobCZart82/Speed-MIDI-Editor
DefaultDirName={localappdata}\Programs\Speed MIDI Editor
DefaultGroupName=Speed MIDI Editor
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.17763
AppMutex=SpeedyMidiAntiUninstall
CloseApplications=no
UninstallDisplayIcon={app}\SpeedMIDIEditor.exe
LicenseFile={#PackageDir}\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=Speed-MIDI-Editor-{#AppVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Speed MIDI Editor"; Filename: "{app}\SpeedMIDIEditor.exe"
Name: "{autodesktop}\Speed MIDI Editor"; Filename: "{app}\SpeedMIDIEditor.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\SpeedMIDIEditor.exe"; Description: "Start Speed MIDI Editor"; Flags: nowait postinstall skipifsilent unchecked
