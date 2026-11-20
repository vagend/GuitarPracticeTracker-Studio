; Inno Setup script for the Windows installer (built by .github/workflows/release.yml):
;   iscc /DAppVersion=0.4.12 /DSourceDir="build\GuitarPracticeStudio_artefacts\Release" packaging\windows-installer.iss
; Installs for the current user only (no admin prompt) into %LOCALAPPDATA%\Programs\Guitar Practice Studio,
; adds a Start-menu entry, and lists the app under Settings > Apps with an Uninstall button.
; Studio's updater runs this with /SILENT; it closes Studio, installs and opens it again.
; Your practice log, settings and plugin choices (in %APPDATA%\Guitar Practice Studio) are kept on uninstall.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
; /DTest builds Guitar Practice Studio Test: a separate app (own install folder, Start-menu entry,
; Apps entry and data) that installs next to the real one.
#ifdef Test
  #define AppName "Guitar Practice Studio Test"
  #define AppGuid "{{6B0C9E52-4D7F-4F7B-9A43-2F3B8E1C7D15}"
  #define FileBase "GuitarPracticeStudioTest"
#else
  #define AppName "Guitar Practice Studio"
  #define AppGuid "{{E2DBE3AF-3CDC-4BE4-B370-99B8F8932DC7}"
  #define FileBase "GuitarPracticeStudio"
#endif
#ifndef SourceDir
  #define SourceDir "..\build\GuitarPracticeStudio_artefacts\Release"
#endif

[Setup]
AppId={#AppGuid}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=vagend
AppPublisherURL=https://github.com/vagend/GuitarPracticeTracker-Studio
DefaultDirName={userpf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename={#FileBase}-setup
UninstallDisplayIcon={app}\{#AppName}.exe
UninstallDisplayName={#AppName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=force
RestartApplications=no

[InstallDelete]
; web files from older builds
Type: filesandordirs; Name: "{app}\web"

[Files]
Source: "{#SourceDir}\{#AppName}.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\web\*"; DestDir: "{app}\web"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userprograms}\{#AppName}"; Filename: "{app}\{#AppName}.exe"
Name: "{userdesktop}\{#AppName}"; Filename: "{app}\{#AppName}.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Put a shortcut on the desktop"; Flags: unchecked

[Run]
; no skipifsilent: after an update (run with /SILENT) Studio opens again
Filename: "{app}\{#AppName}.exe"; Description: "Open {#AppName}"; Flags: nowait postinstall
