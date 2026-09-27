# Gate de calidad — FUSION-HP

> **Regla innegociable**: ningún commit se hace sin gate verde. El gate lo
> ejecuta `build/quality_gate.ps1` automáticamente al final de `build.ps1`,
> y puede ejecutarse manualmente.

## Qué comprueba el gate

### 1. Compilación dual

- Núcleo C++17:
  - `x86` con MSVC (`/MT`, CRT estática): OK
  - `x64` con MSVC (`/MT`, CRT estática): OK
- Capa gestionada:
  - `net35`: OK
  - `net48`: OK
- Qt shell:
  - `x86` con Qt 5.15.2 msvc2019: OK
  - `x64` con Qt 5.15.2 msvc2019_64: OK
- Launcher C++17:
  - `x86`: OK (no hace falta x64 porque es solo bootstrap)
  - `/SUBSYSTEM:windows` (sin consola): OK

### 2. Tests

- **Arnés nativo** (`tests/native/`, doctest):
  - Todos los casos OK.
  - 0 fallos, 0 excepciones no capturadas.
- **Arnés gestionado** (`tests/managed/`, propio sin vstest):
  - Todos los casos OK.
  - 0 fallos.

### 3. Restricciones de seguridad

- **Sin escrituras al Registro**: el gate busca en todo el código C++/C#
  cualquier llamada a `RegCreate*`, `RegSetValue*`, `RegDelete*`. Cero
  ocurrencias permitidas. La única lectura permitida es en el launcher
  (`RegQueryValueEx` para detectar .NET 4.8) y debe estar documentada.
- **Sin comunicación de red**: el gate busca `WinHttp*`, `WinINET*`,
  `socket`, `connect`, `WSAStartup` en el código. Solo permitidos en
  `IpcServer.cpp` con pipe con nombre (no son sockets de red).
- **Sin .cmd/.bat en el producto**: el gate recorre `bin\` y `pack\` y
  falla si encuentra cualquiera de esas extensiones.

### 4. Restricciones de dependencias

- El núcleo C++17 no debe enlazar contra .NET ni contra Qt.
- La capa gestionada net35 no debe usar APIs que no existan en .NET 3.5
  (el gate compila contra las reference assemblies 3.5; cualquier uso
  de APIs modernas rompe esa compilación).
- El Qt shell debe enlazar Qt dinámicamente (DLLs sueltas), no estáticamente.

### 5. Tamaño de binarios

- `FusionCore.exe` (x86): ≤ 12 MB
- `FusionCore.exe` (x64): ≤ 14 MB
- `FusionHP.Managed.exe` (net48): ≤ 8 MB
- Pack portable completo: ≤ 80 MB sin .NET installer, ≤ 150 MB con él.

### 6. Lint y formato

- C++: `clang-format` con `.clang-format` (Google base + 4 espacios).
- C#: `dotnet format` con `.editorconfig`.
- PowerShell: `PSScriptAnalyzer` con regla `PSUseShouldProcessForStateChangingFunctions`.

### 7. Evidencia

- El gate genera `build\artifacts\` con:
  - `binarios_x86.zip`
  - `binarios_x64.zip`
  - `tests_native.log`
  - `tests_managed.log`
  - `gate_report.json`
- Capturas reales (PNG) se suben aparte como artefactos de release, no
  en el repo.

## Cómo correrlo

```powershell
# Build completo + gate
.\build\build.ps1 -Dual

# Solo el gate (asume build ya hecho)
.\build\quality_gate.ps1

# Solo verificación de entorno
.\build\build.ps1 -VerifyEnv
```

## Salida esperada

```
[ OK ] Entorno verificado
[ OK ] Compilación x86 (núcleo)
[ OK ] Compilación x64 (núcleo)
[ OK ] Compilación net35 (gestionada)
[ OK ] Compilación net48 (gestionada)
[ OK ] Compilación x86 (launcher)
[ OK ] Tests nativos: 24/24
[ OK ] Tests gestionados: 18/18
[ OK ] Sin escrituras al Registro
[ OK ] Sin comunicación de red
[ OK ] Sin .cmd/.bat en el producto
[ OK ] Sin dependencias prohibidas
[ OK ] Tamaños dentro de límites
[ OK ] Formato OK

GATE: VERDE
```

Cualquier línea `[ FAIL ]` aborta el commit. El commit se prepara con:

```powershell
.\build\build.ps1 -Dual -PrepareCommit
```

que deja todo en staged y permite revisar antes del `git commit`.
