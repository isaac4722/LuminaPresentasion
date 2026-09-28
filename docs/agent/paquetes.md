# Portable dual e instaladores (CI)

Pieza: `build/empaquetar.ps1` + jobs `portable` / `instaladores` /
`release` de `.github/workflows/ci.yml`.

## Un solo árbol de paquete

`build/empaquetar.ps1 -Arch x86|x64 -Artefactos <dir> -Destino <dir>`
monta el árbol **portable completo** desde los artefactos de Actions:

```
FusionHP.exe           launcher nativo (x86, común a ambas)
FusionCore.exe         núcleo C++17 de la arquitectura pedida
gestionado\            consola C# net48 (exe + dll, sin tests)
qt\                    FusionQtShell.exe + DLL de Qt (windeployqt)
data\                  esquemas, biblia RVR1909, assets y muestras
LEEME.txt              installer/LEEME-portable.txt
LICENSE, THIRD_PARTY_LICENSES.txt, README.md
```

El script busca cada exe con `Get-ChildItem -Recurse` (resistente a
cambios de layout), verifica Qt5Core.dll junto al shell Qt y **falla
con error claro** si falta algo esencial. Sin red, sin Registro, sin
.cmd/.bat (cumple el gate por construcción).

El mismo árbol alimenta **ambos entregables** (una sola fuente de
verdad):

- **Portable**: `Compress-Archive` → `FUSION-HP-portable-<arch>.zip`.
- **Instalador**: `ISCC.exe installer\FusionHP.iss /DARCH=<arch>
  /DPACKAGE=<árbol>` → `FusionHP-Setup-<arch>.exe`.

## CRT estática en todo el producto

Núcleo, launcher y **shell Qt** compilan con `/MT`
(`CMAKE_MSVC_RUNTIME_LIBRARY`): ni el portable ni el instalador
necesitan VC redist. Lo único externo es Qt (LGPL, enlace dinámico,
DLL incluidas) y .NET 4.8 para la consola gestionada (el launcher la
detecta por lectura del Registro y ofrece el instalador offline
opcional en `redist\`).

## Pipeline en Actions

```
nucleo(x86,x64) ─ gestionada ─ qt_shell(x86,x64) ─ launcher
                     └──────────┬──────────────────┘
                                   gate
                  ┌────────────────┴────────────────┐
             portable (x86, x64)            instaladores (ISCC dual)
                  └────────────────┬────────────────┘
                          release (solo tag v*)
                    adjunta 2 instaladores + 2 portables
```

- `portable` y `instaladores` corren **en cada push** a main/develop:
  los artefactos `portable-x86`, `portable-x64` e `instaladores`
  quedan descargables en la página del run (30 días).
- `release` solo con tag `v*`: publica borrador con los 4 archivos y
  notas generadas.
