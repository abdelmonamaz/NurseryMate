; Nursera — Pépinière Idéale : installateur Windows (Inno Setup 6).
; Généré par build_installer.ps1 qui définit AppVer via /DAppVer=x.y.z
; (repli 0.1.0 si compilé à la main).

#ifndef AppVer
  #define AppVer "0.1.0"
#endif

[Setup]
AppName=Nursera — Pépinière Idéale
AppVersion={#AppVer}
AppPublisher=Pépinière Idéale
DefaultDirName={autopf}\Nursera
DefaultGroupName=Nursera
OutputDir=.\Output
OutputBaseFilename=Nursera_Installer_{#AppVer}
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
SetupIconFile=resources\logo.ico

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Files]
Source: "deploy\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Nursera"; Filename: "{app}\nursera-desktop.exe"; IconFilename: "{app}\logo.ico"
Name: "{autodesktop}\Nursera"; Filename: "{app}\nursera-desktop.exe"; IconFilename: "{app}\logo.ico"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Run]
Filename: "{app}\nursera-desktop.exe"; Description: "Lancer Nursera"; Flags: nowait postinstall skipifsilent

; NB : la base de données est créée au premier lancement dans
; %APPDATA%\Pepiniere Ideale\Nursera (aucune donnée de démo embarquée —
; le premier écran est la création du compte Gérant). La désinstallation
; ne touche jamais aux données.
