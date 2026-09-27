# Entorno de build — FUSION-HP

> Requisitos para compilar dual (x86+x64) y dual target (net35+net48) en
> una máquina Windows real. El sandbox Linux NO puede compilar esto.

## Resumen de toolchain

| Componente              | Versión mínima    | Versión probada     |
|-------------------------|-------------------|---------------------|
| Windows                 | 10 1909           | 11 23H2             |
| Visual Studio           | 2019 (16.8)       | 2022 17.x           |
| MSVC toolset            | 14.28             | 14.39               |
| Windows SDK             | 10.0.19041        | 10.0.22621          |
| CMake                   | 3.20              | 3.28                 |
| .NET Framework SDK      | 4.8 + 3.5         | 4.8.1 + 3.5 SP1     |
| Qt                      | 5.15.2             | 5.15.16 (LTS)       |
| Inno Setup              | 6.2               | 6.3                  |
| Python                  | 3.9               | 3.12                 |

## 1. Visual Studio 2022

Instalar con el instalador de VS marcando:

- **Cargas de trabajo**:
  - "Desarrollo para el escritorio con C++"
  - "Desarrollo de aplicaciones de escritorio de .NET" (cubre .NET 4.8)
- **Componentes individuales**:
  - "Compatibilidad con C++ para v142 (VS 2019)" (por si hace falta)
  - "Windows 10 SDK 10.0.19041" o superior
  - "MSVC v143 - VS 2022 C++ x64/x86 build tools"
  - ".NET Framework 4.8 SDK"
  - ".NET Framework 3.5 SDK" (hay que activarlo desde "Componentes
    individuales" — no viene por defecto en VS2022)

Verificar con:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
cl /Bv
```

## 2. Windows SDK

El instalador de VS trae el SDK. Para piso Windows 7 SP1 x86, hay que
instalar adicionalmente el **Windows 7 SDK** (de Visual Studio Installer →
"Componentes individuales → Windows 7 SDK"), porque algunas APIs de
DirectShow solo están ahí. Alternativa: usar el header `dshow.h` del SDK
10 con `_WIN32_WINNT=0x0601`.

## 3. CMake

Descargar de cmake.org e instalar. Verificar:

```powershell
cmake --version
```

## 4. .NET Framework 3.5 + 4.8 SDK

- **3.5**: Panel de control → Programas → Activar o desactivar
  características de Windows → ".NET Framework 3.5 (incluye .NET 2.0 y 3.0)".
  Esto habilita las librerías de referencia 3.5.
- **4.8**: viene con Windows 10 1903+ y VS 2022.

Para que MSBuild encuentre el target `net35`:
`C:\Program Files (x86)\Reference Assemblies\Microsoft\Framework\.NETFramework\v3.5\Profile\Client`
debe existir.

## 5. Qt 5.15.2 LTS

### Instalación con `aqtinstall` (recomendado para CI)

```powershell
pip install aqtinstall
aqt install-qt windows desktop 5.15.2 win32_msvc2019    -m qtcore qtwindowsystem qtwinextras -o C:\Qt\5.15.2
aqt install-qt windows desktop 5.15.2 win64_msvc2019_64  -m qtcore qtwindowsystem qtwinextras -o C:\Qt\5.15.2
```

### Instalación con Qt Online Installer

1. Descargar `qt-online-installer` desde qt.io.
2. Iniciar sesión con cuenta Qt.
3. Seleccionar componente "Qt 5.15.2" → "MSVC 2019 32-bit" y "MSVC 2019 64-bit".
4. Carpeta destino: `C:\Qt\5.15.2`.

### Variables de entorno

```powershell
$env:QTDIR_x86 = "C:\Qt\5.15.2\msvc2019"
$env:QTDIR_x64 = "C:\Qt\5.15.2\msvc2019_64"
```

### Licencia

Qt 5.15.2 se distribuye bajo **LGPL v3**. La integración debe ser
**enlace dinámico** (las DLLs de Qt van sueltas en el pack) y **sin
modificar** los binarios oficiales. Si se modifican, hay que publicar el
código modificado bajo LGPL v3. Ver `THIRD_PARTY_LICENSES.txt`.

## 6. Inno Setup 6.3

Descargar de jrsoftware.org e instalar. Verificar:

```powershell
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /?
```

## 7. .NET 4.8 offline installer (para empaquetar)

Descargar `ndp48-x86-x64-offline.exe` desde:
https://dotnet.microsoft.com/download/dotnet-framework/net48

Colocar en:

```
installer\net48_offline\ndp48-x86-x64-offline.exe
```

(Este archivo **no se sube al repo** por tamaño; ver `.gitignore` y
`installer\net48_offline\README.txt`.)

## 8. Librerías de terceros (núcleo)

Descargar cada una como submódulo git en `src/core/third_party/` o
gestionar con vcpkg. Pendiente: definir. Para el primer commit, las
cabeceras de `wil`, `nlohmann/json`, `spdlog` y `doctest` se incluyen
como single-header en `src/core/third_party/` y la amalgama de SQLite en
`src/core/third_party/sqlite/sqlite3.c`.

| Librería        | Cómo se incluye                                    |
|-----------------|----------------------------------------------------|
| wil             | submodule en `src/core/third_party/wil`            |
| spdlog          | submodule en `src/core/third_party/spdlog`         |
| doctest         | header solo en `src/core/third_party/doctest.h`    |
| nlohmann/json    | header solo en `src/core/third_party/json.hpp`     |
| SQLite          | amalgama en `src/core/third_party/sqlite/sqlite3.c`|

## 9. Librerías de terceros (capa gestionada)

Vía NuGet:

```xml
<PackageReference Include="Newtonsoft.Json" Version="13.0.3" />
<PackageReference Include="NLog" Version="5.3.4" />
<PackageReference Include="PdfSharp" Version="6.1.1" />
<PackageReference Include="System.Data.SQLite.Core" Version="1.0.118" />
<PackageReference Include="Ookii.Dialogs.WinForms" Version="4.0.0" />
<PackageReference Include="BouncyCastle.Cryptography" Version="2.4.0" />
<PackageReference Include="DocumentFormat.OpenXml" Version="2.19.0" />
```

## 10. Verificación del entorno

```powershell
.\build\build.ps1 -VerifyEnv
```

Debe imprimir `OK` para: VS2022, Windows SDK, CMake, .NET 3.5, .NET 4.8,
Qt 5.15.2 (x86 y x64), Inno Setup. Si algo falla, ver mensajes específicos
del script.
