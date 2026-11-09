[Setup]
AppName=OpenClip
AppVersion=0.2
DefaultDirName={autopf}\OpenClip
DefaultGroupName=OpenClip
OutputDir=Output
OutputBaseFilename=OpenClip-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\openclip.exe

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\OpenClip"; Filename: "{app}\openclip.exe"
Name: "{autodesktop}\OpenClip"; Filename: "{app}\openclip.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"

[Run]
Filename: "{app}\openclip.exe"; Description: "Launch OpenClip"; Flags: nowait postinstall skipifsilent
