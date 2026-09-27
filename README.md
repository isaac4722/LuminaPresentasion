# FUSION-HP

Aplicación híbrida de presentación litúrgica y multimedia para Windows.

> **Piso mínimo**: Windows 7 SP1 x86 · **Hasta**: Windows 11 x64
> **100 % offline** — sin nube, sin cuentas, sin telemetría, sin control remoto.

## Qué hace

Proyecta cultos y actos de iglesia: letras de cantos, Biblia, imágenes, vídeo,
presentaciones PPTX y avisos. Pensada para iglesias que necesitan una
herramienta local, sin dependencias de red y sin requisitos de compilación para
el usuario final (extrae el pack y hace doble clic).

## Estado del repositorio

Este es el commit **fundacional**: contiene la estructura completa del
monorepo, los formatos versionados (`ahp.v1`, `ipc.v1`), los esquemas SQLite
(`cancionero.fdb`, biblias), los importadores, el launcher nativo, el
instalador Inno Setup dual y el CI de GitHub Actions.

La lógica de negocio se irá añadiendo por piezas, siguiendo las prioridades P0
→ P1 → P2 definidas en `AGENT.md`.

## Estructura

```
src/core       C++17 — núcleo nativo (motor de proyección)
src/launcher   C++17 — arranque nativo sin consola
src/managed    C# .NET Framework net35+net48 — carcasa principal
src/qt-shell   Qt 5.15.2 — tercera carcasa opcional
build/         Scripts de build (PowerShell)
installer/     Inno Setup dual x86/x64
data/          Esquemas, samples, biblias, assets
tests/         Arneses de pruebas
docs/agent/    Documentación interna
```

## Cómo construir (en Windows)

Requisitos: ver `docs/agent/build_environment.md`.

```powershell
# Desde la raíz del repo
.\build\build.ps1 -Dual
```

Esto compila x86 y x64 del núcleo C++, y net35+net48 de la capa gestionada,
corre ambos arneses de tests y ejecuta el gate de calidad.

## Cómo arrancar (usuario final)

1. Extraer el pack portable (x86 o x64 según la máquina).
2. Doble clic en `FusionHP.exe` (el launcher nativo).
   - Si falta .NET 4.8, el launcher instala el runtime offline oficial
     incluido una sola vez, con aviso, y luego abre el programa.
3. A usar.

## Documentación

- `AGENT.md` — autoridad máxima del proyecto.
- `docs/agent/` — specs internos (formatos, arquitectura, build, calidad).

## Licencia

Propietaria. Ver `LICENSE`. Las librerías de terceros conservan sus licencias
originales (ver `THIRD_PARTY_LICENSES.txt`).
