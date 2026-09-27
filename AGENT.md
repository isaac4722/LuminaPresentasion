# AGENT.md — Reglas del agente de desarrollo · FUSION-HP

> Este archivo es la **autoridad máxima** del repositorio. Toda decisión técnica,
> de proceso o de calidad se resuelve aquí. En caso de conflicto con cualquier
> otro documento, gana AGENT.md.

## 1. Identidad del proyecto

**FUSION-HP** es una aplicación híbrida de presentación litúrgica y multimedia
para Windows (piso mínimo: Windows 7 SP1 x86; hasta Windows 11 x64). Proyecta
cultos y actos de iglesia: letras de cantos, Biblia, imágenes, vídeo,
presentaciones y avisos.

Es un producto **100 % local**: sin nube, sin cuentas, sin telemetría y sin
ninguna comunicación de red. La única comunicación permitida es el **IPC interno**
entre los ejecutables del propio programa (`ipc.v1`).

El usuario final **nunca compila nada**: extrae el pack y hace doble clic.

## 2. Stack tecnológico innegociable

| Capa | Tecnología | Notas |
|------|------------|------|
| Núcleo | C++17 compilado con MSVC, **CRT estática** (`/MT`) | Dueño del estado de proyección |
| Gestiona | C# sobre .NET Framework, **dual target net35+net48** | Carcasa de interfaz principal |
| Qt shell | Qt 5.15.2 (LGPL), **enlace dinámico**, DLL oficiales sin modificar | Tercera carcasa opcional |
| Render | Direct2D + DirectWrite + GDI+ | Salida sin parpadeo |
| Vídeo | DirectShow (VMR-9 sin ventanas) | |
| Datos | SQLite embebido (amalgama) | `cancionero.fdb` + biblias |
| Format | `ahp.v1` (JSON versionado), `ipc.v1` (IPC JSON) | |
| Build | MSBuild + CMake + Inno Setup | Cero .cmd/.bat |
| CI | GitHub Actions sobre `windows-latest` | Tag `v*` arma release |

## 3. Reglas innegociables

1. **100 % offline**: sin API, sin control remoto, sin nube, sin telemetría; la
   única comunicación es el IPC interno del programa.
2. **Sin escrituras al Registro de Windows**; configuración y datos en archivos
   junto al ejecutable.
3. **Piso Windows 7 SP1 x86**; compilación dual x86+x64 obligatoria; **CRT
   estática** en el núcleo.
4. La capa compartida debe seguir compilando en **.NET 3.5** (prohibidas APIs
   modernas ahí); targets duales `net35`+`net48`.
5. **Cero .cmd/.bat** en el producto: el arranque es 100 % ejecutables nativos.
6. Sin Java, sin .NET Core, sin navegadores embebidos (nada de
   Chromium/CEF/WebView).
7. **El núcleo es el dueño del estado de proyección**: cerrar la interfaz no
   tira la proyección; cerrar la app no deja procesos vivos.
8. **Método de trabajo**: ciclo auditar → corregir → compilar → capturar → leer
   → corregir; `1 pieza = 1 commit` (convencionales, en español); gate de
   calidad verde antes de cada commit; build dual + tests de ambos arneses.
9. **Evidencia honesta**: capturas reales de Inicio/Estudio/Presentar/salida;
   prohibido inventar cifras, resultados o pruebas no ejecutadas.

## 4. Estructura del repositorio

```
LuminaPresentasion/
├── .github/workflows/        # CI de GitHub Actions
├── AGENT.md                  # ESTE ARCHIVO — autoridad máxima
├── docs/agent/               # Documentación interna del agente
│   ├── format_ahp_v1.md      # Spec del formato ahp.v1
│   ├── format_ipc_v1.md      # Spec del protocolo ipc.v1
│   ├── architecture.md       # Arquitectura del sistema
│   ├── build_environment.md  # Cómo preparar el toolchain
│   ├── quality_gate.md       # Reglas del gate de calidad
│   └── conventions.md        # Convenciones de código y commit
├── src/
│   ├── core/                 # C++17 — núcleo nativo (Motor de proyección)
│   ├── launcher/             # C++17 — arranque nativo sin consola
│   ├── managed/              # C# .NET Framework net35+net48 — carcasa principal
│   └── qt-shell/             # Qt 5.15.2 — tercera carcasa opcional
├── build/                    # Scripts de build (PowerShell + MSBuild props)
├── installer/                # Inno Setup (.iss) dual x86+x64
├── data/
│   ├── schema/               # Esquemas SQLite (.sql)
│   ├── samples/              # Ejemplos de formatos
│   ├── bibles/               # Biblias (4, dominio público / libre distribución)
│   └── assets/               # Fuentes, iconos, fondos, logo
└── tests/                    # Arneses de pruebas (nativo + gestionado)
```

## 5. Definición de "Hecho" (por pieza)

Antes de marcar una pieza como completada, **todos** los siguientes deben estar
verificados en una máquina Windows real:

- [ ] Compila dual: x86 **y** x64 con MSVC, `net35` **y** `net48` con dotnet/MSBuild.
- [ ] Tests del arnés nativo en verde.
- [ ] Tests del arnés gestionado en verde.
- [ ] Gate de calidad verde (`build/quality_gate.ps1`).
- [ ] Evidencia con captura real de pantalla.
- [ ] Bitácora actualizada (`worklog.md`).
- [ ] Commit propio (1 pieza = 1 commit, mensaje en convención española).

## 6. Prioridades del producto

| Prio | Qué |
|------|-----|
| **P0** | Abrir/crear/guardar `ahp.v1` con recientes; biblioteca completa (`cancionero.fdb` + 4 biblias); proyectar BD→Motor sin archivos intermedios; navegación con flechas/Espacio, B/C/L y Esc; selección de monitor con salida borderless visible; cierre limpio sin procesos residuales; importadores Holyrics JSON, e-Sword .bib, Zefania XML y TSV. |
| **P1** | Temas en caliente; Biblia rápida G + favoritos + Tercio; acordes con transposición; Stage View; historial + CSV; miniaturas; PPTX original (COM o directo); exportadores PPTX/PDF/PNG. |
| **P2** | Herencia de 4 niveles con informe de fidelidad; .pptm sin macros; búsqueda ≤200 ms; autotest diagnóstico; log de 60 min sin ERROR; límites de recursos documentados. |

## 7. Convenciones de commit

Formato: **Convencionales en español**.

```
tipo(ámbito): descripción breve en presente

- detalle 1
- detalle 2
```

**Tipos permitidos:**

- `feat`: nueva funcionalidad
- `fix: corrección de bug
- `refactor`: refactor sin cambio de comportamiento
- `docs: documentación
- `build: scripts de build, CI, installer
- `test: pruebas
- `chore: tareas de mantenimiento
- `perf: mejora de rendimiento

**Ámbitos:** `nucleo`, `lanzador`, `gestionada`, `qt`, `ipc`, `datos`,
`importadores`, `installer`, `ci`, `docs`, `calidad`.

**Ejemplos:**

```
feat(nucleo): salida borderless con selección de monitor por nombre

- Implementa ProjectionWindow::MostrarEn(MonitorId)
- Persiste el monitor elegido en sesión JSON
- Cero parpadeo al reaparecer la salida
```

```
fix(importadores): parses Zefania XML con namespaces variables
```

## 8. Ciclo de trabajo por pieza

```
auditar → corregir → compilar → capturar → leer → corregir
   ↑                                              ↓
   └──────────── loop hasta verde ←──────────────┘
```

1. **Auditar**: leer el estado actual del repo, `worklog.md`, y la pieza a tocar.
2. **Corregir**: aplicar el cambio mínimo y limpio.
3. **Compilar**: `build.ps1 -Dual` (debe dar verde en x86, x64, net35 y net48).
4. **Capturar**: screenshot real de Inicio/Estudio/Presentar/salida según
   corresponda.
5. **Leer**: revisar logs, tests, métricas.
6. **Corregir**: si algo no está verde, volver al paso 2.
7. Cuando todo está verde → commit + push.

## 9. Restricciones del sandbox de desarrollo

Este agente se ejecuta en un **sandbox Linux**. Las siguientes restricciones
aplican y deben documentarse en cada commit que no pueda verificarlas:

- No se puede compilar código MSVC dual aquí (no hay Visual Studio).
- No se pueden correr tests nativos C++ aquí (no hay Windows).
- No se pueden tomar capturas reales aquí.
- Toda compilación y verificación debe hacerse en una máquina Windows con el
  toolchain descrito en `docs/agent/build_environment.md`.

**Cuando se trabaja en este sandbox**, el agente:

- Genera código fuente que **debería** compilar dual y pasar tests.
- Deja explícito en `worklog.md` qué quedó verificado y qué quedó pendiente.
- Nunca inventa resultados de tests ni capturas. Si no se ejecutó, se dice "no
  ejecutado en este entorno".

## 10. Archivos fuente de este documento

- `docs/agent/architecture.md` — diseño técnico detallado.
- `docs/agent/format_ahp_v1.md` — formato de programas de culto.
- `docs/agent/format_ipc_v1.md` — protocolo IPC interno.
- `docs/agent/build_environment.md` — preparación del toolchain.
- `docs/agent/quality_gate.md` — gate de calidad.
- `docs/agent/conventions.md` — convenciones de código.

Para cualquier duda no resuelta aquí, mandan `AGENT.md` y `docs/agent/`.
