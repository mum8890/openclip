#ifndef AppVer
#define AppVer "0.6.0"
#endif

[Setup]
AppId={{8F3C2A51-6B7D-4E1A-9C54-0D2E7B1A93F6}
AppName=OpenClip
AppPublisher=Ghostreel
AppPublisherURL=https://github.com/mum8890/openclip
AppSupportURL=https://github.com/mum8890/openclip/issues
AppCopyright=Ghostreel
AppVersion={#AppVer}
CloseApplications=yes
RestartApplications=yes
DefaultDirName={autopf}\OpenClip
DefaultGroupName=OpenClip
OutputDir=Output
OutputBaseFilename=OpenClip-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\openclip.exe
SetupIconFile=..\resources\icon.ico

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\OpenClip"; Filename: "{app}\openclip.exe"
Name: "{autodesktop}\OpenClip"; Filename: "{app}\openclip.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"

[Run]
Filename: "{app}\openclip.exe"; Description: "Launch OpenClip"; Flags: nowait postinstall skipifsilent
