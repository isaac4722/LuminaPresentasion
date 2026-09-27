; ============================================================================
; FusionHP.iss — Instalador Inno Setup dual x86 + x64
; Genera installer/Output/FusionHP-Setup-{arch}.exe
; Definir /DARCH=x86 o /DARCH=x64 al invocar ISCC.exe
; ============================================================================

#ifndef ARCH
  #define ARCH "x86"
#endif

#if ARCH == "x64"
  #define ArchSuffix "x64"
  #define Platform "x64"
#else
  #define ArchSuffix "x86"
  #define Platform "x86"
#endif

[Setup]
AppName=FUSION-HP
AppVersion=1.0.0
AppPublisher=FUSION-HP
AppPublisherURL=https://github.com/isaac4722/LuminaPresentasion
DefaultDirName={pf}\FUSION-HP
DefaultGroupName=FUSION-HP
AllowNoIcons=yes
LicenseFile=..\LICENSE
OutputDir=Output
OutputBaseFilename=FusionHP-Setup-{#ArchSuffix}
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitOS={#Platform}
ArchitecturesAllowed={#Platform}
DisableProgramGroupPage=yes
DisableDirPage=no
PrivilegesRequired=admin
UninstallDisplayIcon={app}\FusionHP.exe
WizardStyle=modern

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear icono en el escritorio"; GroupDescription: "Iconos:"
Name: "quicklaunchicon"; Description: "Crear icono en la barra de tareas"; GroupDescription: "Iconos:"; OnlyBelowVersion: 0,6.1

[Files]
; Launcher
Source: "..\src\launcher\build\bin\FusionHP.exe"; DestDir: "{app}"; Flags: ignoreversion

; Núcleo
Source: "..\src\core\build-{#ArchSuffix}\bin\FusionCore.exe"; DestDir: "{app}"; Flags: ignoreversion

; Capa gestionada
Source: "..\src\managed\FusionHP.Managed\bin\Release\net48\FusionHP.Managed.exe"; DestDir: "{app}\managed"; Flags: ignoreversion
Source: "..\src\managed\FusionHP.Managed\bin\Release\net48\*.dll"; DestDir: "{app}\managed"; Flags: ignoreversion recursesubdirs createallsubdirs

; Qt shell (si se compiló para esta arquitectura)
#if ARCH == "x64"
  Source: "..\src\qt-shell\build-x64\FusionQtShell.exe"; DestDir: "{app}\qt"; Flags: ignoreversion skipifsourcedoesntexist
  Source: "C:\Qt\5.15.2\msvc2019_64\bin\Qt5Core.dll";  DestDir: "{app}\qt"; Flags: ignoreversion skipifsourcedoesntexist
  Source: "C:\Qt\5.15.2\msvc2019_64\bin\Qt5Gui.dll";   DestDir: "{app}\qt"; Flags: ignoreversion skipifsourcedoesntexist
  Source: "C:\Qt\5.15.2\msvc2019_64\bin\Qt5Widgets.dll"; DestDir: "{app}\qt"; Flags: ignoreversion skipifsourcedoesntexist
  Source: "C:\Qt\5.15.2\msvc2019_64\plugins\platforms\qwindows.dll"; DestDir: "{app}\qt\platforms"; Flags: ignoreversion skipifsourcedoesntexist
#else
  Source: "..\src\qt-shell\build-x86\FusionQtShell.exe"; DestDir: "{app}\qt"; Flags: ignoreversion skipifsourcedoesntexist
  Source: "C:\Qt\5.15.2\msvc2019\bin\Qt5Core.dll";  DestDir: "{app}\qt"; Flags: ignoreversion skipifsourcedoesntexist
  Source: "C:\Qt\5.15.2\msvc2019\bin\Qt5Gui.dll";   DestDir: "{app}\qt"; Flags: ignoreversion skipifsourcedoesntexist
  Source: "C:\Qt\5.15.2\msvc2019\bin\Qt5Widgets.dll"; DestDir: "{app}\qt"; Flags: ignoreversion skipifsourcedoesntexist
  Source: "C:\Qt\5.15.2\msvc2019\plugins\platforms\qwindows.dll"; DestDir: "{app}\qt\platforms"; Flags: ignoreversion skipifsourcedoesntexist
#endif

; Datos
Source: "..\data\schema\*"; DestDir: "{app}\data\schema"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\data\bibles\*.fdb"; DestDir: "{app}\data\bibles"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\data\bibles\README.md"; DestDir: "{app}\data\bibles"; Flags: ignoreversion
Source: "..\data\assets\*"; DestDir: "{app}\data\assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\data\samples\*"; DestDir: "{app}\data\samples"; Flags: ignoreversion

; Licencias
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_LICENSES.txt"; DestDir: "{app}"; Flags: ignoreversion

; .NET 4.8 offline installer (lo deja al lado; lo arranca el launcher si falta)
Source: "net48_offline\ndp48-x86-x64-offline.exe"; DestDir: "{app}\redist"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\FUSION-HP"; Filename: "{app}\FusionHP.exe"
Name: "{group}\Desinstalar FUSION-HP"; Filename: "{uninstallexe}"
Name: "{commondesktop}\FUSION-HP"; Filename: "{app}\FusionHP.exe"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar\FUSION-HP"; Filename: "{app}\FusionHP.exe"; Tasks: quicklaunchicon

[Run]
; El launcher detecta .NET 4.8 por sí mismo y arranca el instalador si falta.
; No lo forzamos aquí para respetar el piso Win7 SP1.
Filename: "{app}\FusionHP.exe"; Description: "Iniciar FUSION-HP"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\runtime"
Type: filesandordirs; Name: "{app}\logs"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
