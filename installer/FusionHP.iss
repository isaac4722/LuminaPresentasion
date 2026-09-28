; ============================================================================
; FusionHP.iss — Instalador Inno Setup dual x86 + x64 de FUSION-HP
; Genera installer/Output/FusionHP-Setup-{arch}.exe
;
; Al invocar ISCC.exe:
;   /DARCH=x86 | x64          → nombre de salida y arquitectura de destino
;   /DPACKAGE=<ruta>          → árbol montado por build/empaquetar.ps1
;                                (mismo árbol que el portable: una sola
;                                fuente de verdad)
; Ejemplo (CI o local):
;   ISCC.exe installer\FusionHP.iss /DARCH=x64 /DPACKAGE=build\paquete\x64
; ============================================================================

#ifndef ARCH
  #define ARCH "x86"
#endif

#ifndef PACKAGE
  #define PACKAGE "paquete"
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

[Files]
; Árbol de paquete montado por build/empaquetar.ps1 (mismo que el portable).
Source: "{#PACKAGE}\FusionHP.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PACKAGE}\FusionCore.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PACKAGE}\gestionado\*"; DestDir: "{app}\gestionado"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PACKAGE}\qt\*"; DestDir: "{app}\qt"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PACKAGE}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PACKAGE}\LEEME.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PACKAGE}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PACKAGE}\THIRD_PARTY_LICENSES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PACKAGE}\README.md"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Instalador offline de .NET 4.8 (opcional; lo arranca el launcher si falta)
Source: "net48_offline\ndp48-x86-x64-offline.exe"; DestDir: "{app}\redist"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\FUSION-HP"; Filename: "{app}\FusionHP.exe"
Name: "{group}\FUSION-HP (shell Qt)"; Filename: "{app}\qt\FusionQtShell.exe"
Name: "{group}\Desinstalar FUSION-HP"; Filename: "{uninstallexe}"
Name: "{commondesktop}\FUSION-HP"; Filename: "{app}\FusionHP.exe"; Tasks: desktopicon

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
