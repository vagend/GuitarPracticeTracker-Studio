; Inno Setup script for the Windows installer (built by .github/workflows/release.yml):
;   iscc /DAppVersion=0.4.12 /DSourceDir="build\GuitarPracticeStudio_artefacts\Release" packaging\windows-installer.iss
; Installs for the current user only (no admin prompt) into %LOCALAPPDATA%\Programs\Guitar Practice Studio,
; adds a Start-menu entry, and lists the app under Settings > Apps with an Uninstall button.
; Studio's updater runs this with /SILENT; it closes Studio, installs and opens it again.
; Your practice log, settings and plugin choices (in %APPDATA%\Guitar Practice Studio) are kept on uninstall.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\build\GuitarPracticeStudio_artefacts\Release"
#endif

[Setup]
AppId={{E2DBE3AF-3CDC-4BE4-B370-99B8F8932DC7}
AppName=Guitar Practice Studio
AppVersion={#AppVersion}
AppVerName=Guitar Practice Studio {#AppVersion}
AppPublisher=vagend
AppPublisherURL=https://github.com/vagend/GuitarPracticeTracker-Studio
DefaultDirName={userpf}\Guitar Practice Studio
DefaultGroupName=Guitar Practice Studio
DisableProgramGroupPage=yes
DisableDirPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=GuitarPracticeStudio-setup
UninstallDisplayIcon={app}\Guitar Practice Studio.exe
UninstallDisplayName=Guitar Practice Studio
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=force
RestartApplications=no

[InstallDelete]
; web files from older builds
Type: filesandordirs; Name: "{app}\web"

[Files]
Source: "{#SourceDir}\Guitar Practice Studio.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\web\*"; DestDir: "{app}\web"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userprograms}\Guitar Practice Studio"; Filename: "{app}\Guitar Practice Studio.exe"
Name: "{userdesktop}\Guitar Practice Studio"; Filename: "{app}\Guitar Practice Studio.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Put a shortcut on the desktop"; Flags: unchecked

[Run]
; no skipifsilent: after an update (run with /SILENT) Studio opens again
Filename: "{app}\Guitar Practice Studio.exe"; Description: "Open Guitar Practice Studio"; Flags: nowait postinstall
