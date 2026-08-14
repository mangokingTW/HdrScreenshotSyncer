; Inno Setup script for HdrScreenshotSyncer. One per-user installer (the tool
; never needs elevation), x64 only -- it refuses to run on 32-bit Windows.
; Autostart is an opt-in task writing the same HKCU Run entry the app's tray
; toggle manages.

#define AppName "HdrScreenshotSyncer"
#define AppExeName "HdrScreenshotSyncer.exe"
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
; Full string may carry a pre-release suffix; VersionInfoVersion needs numeric.
#ifndef AppVersionFull
  #define AppVersionFull AppVersion
#endif

[Setup]
AppId={{A345CF69-EF8A-4B6D-BF82-25F336A73950}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=mangokingTW
AppPublisherURL=https://github.com/mangokingTW/HdrScreenshotSyncer
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
PrivilegesRequired=lowest
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename={#AppName}-{#AppVersionFull}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
VersionInfoVersion={#AppVersion}
UninstallDisplayIcon={app}\{#AppExeName}

[Tasks]
Name: "startup"; Description: "Start automatically at logon"

[Files]
Source: "..\build-x64\Release\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; \
  ValueName: "{#AppName}"; ValueData: """{app}\{#AppExeName}"""; \
  Flags: uninsdeletevalue; Tasks: startup

[Run]
Filename: "{app}\{#AppExeName}"; Flags: nowait postinstall skipifsilent
