# Arquitectura — FUSION-HP

## Visión general

El sistema se organiza en **cuatro capas** con responsabilidades estrictamente
separadas. La separación clave es: **el núcleo es el dueño del estado de
proyección**, y las carcasas de interfaz son clientes que lo operan vía IPC.

```
┌──────────────────────────────────────────────────────────────────┐
│                  Capa de interfaz (carcasas)                     │
│  ┌─────────────────┐  ┌─────────────────┐  ┌────────────────┐  │
│  │ C# .NET (win32) │  │ Qt 5.15.2 shell │  │ Operador F8   │  │
│  │  Inicio/Estudio │  │  (opcional)     │  │  (consola)     │  │
│  │  Presentar/...  │  │                 │  │                │  │
│  └────────┬────────┘  └────────┬────────┘  └────────┬───────┘  │
│           │                    │                    │           │
│           └────────────────────┼────────────────────┘           │
│                                │                                │
│                         IPC interno (ipc.v1)                    │
│                                │ JSON por pipe con nombre       │
└────────────────────────────────┼────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                  Núcleo nativo (C++17, MSVC, /MT)               │
│                                                                 │
│  ┌──────────────┐  ┌───────────────┐  ┌───────────────────┐   │
│  │   Engine     │←→│   Renderer    │  │  ProjectionWindow │   │
│  │  (estado)    │  │ Direct2D/DW   │  │  borderless FS    │   │
│  └──────┬───────┘  └───────────────┘  └───────────────────┘   │
│         │                                                       │
│  ┌──────▼───────┐  ┌───────────────┐  ┌───────────────────┐   │
│  │  IpcServer   │  │  VideoPlayer  │  │     Session       │   │
│  │  (pipe srv)  │  │ DirectShow    │  │  (JSON persist.)  │   │
│  └──────────────┘  └───────────────┘  └───────────────────┘   │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              Datos (SQLite embebido)                     │  │
│  │  cancionero.fdb  ·  4 biblias  ·  historial JSONL         │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
```

## Componentes

### Núcleo nativo (`src/core/`)

Compilado con MSVC, C++17, **`/MT` (CRT estática)**, dual x86+x64. Es un
ejecutable sin interfaz propia que se ejecuta como proceso background.

- **`Engine`**: dueño del estado de proyección. Mantiene el programa activo,
  el escenario actual, el elemento actual y la línea actual. Persiste la
  sesión a `session.json` para sobrevivir cierres de la carcasa.
- **`Renderer`**: pipeline Direct2D + DirectWrite + GDI+. Sin parpadeo
  (doble buffer + composición por hardware cuando esté disponible).
- **`ProjectionWindow`**: ventana borderless a pantalla completa en el
  monitor seleccionado. Nace oculta, solo aparece al iniciar la
  presentación. Nunca se destruye entre usos.
- **`VideoPlayer`**: DirectShow con VMR-9 sin ventanas.
- **`IpcServer`**: pipe con nombre (`\\.\pipe\FusionHP-ipc`). Solo
  conexiones locales del propio programa.
- **`Session`**: persiste el estado a `runtime/session.json`.

### Launcher (`src/launcher/`)

Ejecutable nativo C++17 sin consola (`WinMain`, `/SUBSYSTEM:windows`).

1. Comprueba si .NET 4.8 está instalado (lectura de
   `HKLM\SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full\Release` — solo
   lectura, **no escritura**).
2. Si falta y existe `installer\net48_offline\ndp48-x86-x64-offline.exe`,
   lo arranca con aviso al usuario (una sola vez, marcado en
   `runtime\net48_installed.flag`).
3. Arranca el proceso del núcleo (`FusionCore.exe`) si no está corriendo.
4. Arranca la carcasa gestionada (`FusionHP.Managed.exe`).
5. Sale sin dejar procesos hijos colgados.

### Capa gestionada (`src/managed/`)

C# sobre .NET Framework, **dual target `net35`+`net48`**. Una sola
carpeta de proyecto produce dos DLLs/exes (`net35` y `net48`).

- `MainForm` (Inicio): tarjetas, recientes, configuración.
- `StudioForm` (Estudio): editor de programas `ahp.v1`.
- `PresentForm` (Presentar): consola de proyección.
- `LibraryForm` (Biblioteca): cantos y biblias.
- `OperatorConsoleForm` (F8): consola en vivo EN PANTALLA + SIGUIENTE.
- `BibleQuickForm` (G): búsqueda bíblica rápida.
- `DiagnosticForm` (Ayuda → Estado del sistema): autotest.

La capa gestionada **nunca** toca Direct2D ni la salida. Toda operación
de proyección pasa por IPC al núcleo.

### Qt shell (`src/qt-shell/`)

Tercera carcasa opcional. Qt 5.15.2 LGPL, **enlace dinámico**, DLL
oficiales sin modificar. Mismo protocolo IPC que la carcasa gestionada.

## Comunicación

Toda comunicación es **IPC interno JSON** (ver `format_ipc_v1.md`):

- Pipe con nombre `\\.\pipe\FusionHP-ipc`, solo local.
- Mensajes JSON con `type`, `id`, `payload`.
- Respuestas sincrónicas con `id` correlativo.
- Sin timeouts infinitos: 5 s por defecto, 30 s para operaciones largas.

**No hay ninguna otra comunicación de red en todo el producto.**

## Datos

### cancionero.fdb

SQLite embebido. Esquema en `data/schema/cancionero.sql`. Cargado una sola
vez al arrancar el núcleo. Una sola base para todos los cantos (sin tope
de registros, sin búsqueda obligatoria para listar).

### Biblias

SQLite embebido. Esquema en `data/schema/bible.sql`. 4 biblias
preinstaladas, cada una en su archivo `.fdb` dentro de `data/bibles/`.

### session.json

Estado del motor: programa activo, escenario, elemento, línea, monitor,
tema aplicado. Permite que cerrar la carcasa no tire la proyección y que
al reabrir se resincronice todo.

### historial.jsonl

JSONL append-only. Una línea por elemento proyectado. Usado por el
historial de servicios y la lista "más usadas".

## Ciclo de vida de procesos

```
arranque:
  launcher.exe
    └─ comprueba .NET 4.8 (instala si falta)
    └─ arranca FusionCore.exe (núcleo, proceso background)
    └─ arranca FusionHP.Managed.exe (carcasa, proceso UI)
    └─ sale limpio

uso normal:
  carcasa (UI) ↔ núcleo (IPC) ↔ salida (pantalla)
   │
   └─ si se cierra la carcasa: el núcleo sigue proyectando
   └─ si se reabre la carcasa: lee session.json y se resincroniza

cierre limpio:
  carcasa → IPC Cerrar() al núcleo
  núcleo → persiste session.json, libera recursos, sale
  launcher ya salió antes → no quedan procesos vivos
```

## Restricciones de diseño

- **Sin parpadeo**: la ventana de proyección nunca se destruye. Se oculta
  con `ShowWindow(SW_HIDE)` y se reutiliza.
- **Sin escrituras al Registro**: todo va a `runtime\` junto al exe.
- **Sin dependencias de red**: no hay `WinHttp`, `WinINET`, sockets, ni
  nada que pueda alcanzar la red. Si una librería de terceros lo trae,
  se quita.
- **Sin procesos hijos zombis**: el launcher usa `Job Objects` para
  garantizar que si el launcher muere, los hijos también.

## Extensiones futuras

- Stage View (P1): monitor de músicos, sale del núcleo por IPC.
- Exportadores PPTX/PDF/PNG (P1): viven en la capa gestionada sobre
  DocumentFormat.OpenXml y PdfSharp.
- Importación PPTX → escenarios (P1): vive en la capa gestionada.
