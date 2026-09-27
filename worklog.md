# Worklog — FUSION-HP

Bitácora compartida de trabajo. Append-only. Cada agente lee esto antes
de empezar y añade su sección al terminar.

---
Task ID: 0
Agent: Agente IA (Claude/GLM via Super Z)
Task: Commit fundacional del repo. Cimientos + CI completo, esquemas
SQLite, formatos ahp.v1 e ipc.v1, launcher nativo, Inno Setup, GitHub
Actions dual, todo en español.

Work Log:
- Clonado el repo vacío en /home/z/my-project/work/LuminaPresentasion
- Aclarado con el usuario el alcance del primer commit (Cimientos + CI,
  monorepo con carpetas, CI completo, restricciones estrictas, 1 commit
  fundacional en español).
- Creada la estructura completa del monorepo (src/core, src/launcher,
  src/managed, src/qt-shell, build, installer, data, tests, docs/agent).
- Escritos los archivos raíz: AGENT.md (autoridad máxima), README.md,
  LICENSE, THIRD_PARTY_LICENSES.txt, .gitignore, .editorconfig.
- Escrita la documentación interna del agente en docs/agent/ (7
  documentos: README, architecture, format_ahp_v1, format_ipc_v1,
  build_environment, quality_gate, conventions).
- Escritos los esquemas SQLite: data/schema/cancionero.sql (cantos,
  secciones, líneas, etiquetas, favoritos) y data/schema/bible.sql
  (libros, capítulos, versículos, FTS5, semilla de los 66 libros).
- Escritos los samples: ahp_v1_example.ahp, holyrics_sample.json,
  bible_sample.json.
- Escrito el workflow de GitHub Actions (.github/workflows/ci.yml) con
  jobs separados para núcleo (dual x86+x64), gestionada (net35+net48),
  Qt shell (dual), launcher (x86) y gate de calidad.
- Escritos los scripts de build: build/build.ps1, build/build_props.props,
  build/quality_gate.ps1 (verifica sin escrituras al Registro, sin
  comunicación de red, sin .cmd/.bat, sin dependencias prohibidas).
- Escrito el instalador Inno Setup dual (installer/FusionHP.iss) +
  installer/net48_offline/README.txt documentando dónde poner el
  instalador offline de .NET 4.8.
- Escrito el núcleo C++17 completo:
  - Headers públicos en src/core/include/fusion/ (Engine, Session,
    Renderer, ProjectionWindow, IpcServer, VideoPlayer, SongDatabase,
    BibleDatabase, AhpFormat, 5 importadores).
  - Implementaciones en src/core/src/ (stubs fundacionales que
    dejan los tipos y el API bien definidos; lógica de negocio va
    pieza por pieza en commits siguientes).
  - CMakeLists.txt con /MT (CRT estática), dual x86+x64, target
    opcional de tests.
  - Placeholders de terceros (sqlite3.c/.h, json.hpp) que documentan
    cómo reemplazarlos por las versiones reales antes de compilar.
- Escrito el launcher nativo C++17 (src/launcher/): WinMain sin consola,
  detección de .NET 4.8 por lectura del Registro (solo lectura, sin
  escrituras), arranque del instalador offline si falta, Job Objects
  para garantizar cierre limpio sin procesos hijos zombis.
- Escrita la capa gestionada C# (src/managed/):
  - Solution FusionHP.Managed.sln con dos proyectos: gestionada y tests.
  - .csproj con dual target net35+net48 vía build_props.props.
  - 7 forms esqueleto (MainForm/Inicio, StudioForm, PresentForm,
    LibraryForm, OperatorConsoleForm, BibleQuickForm, DiagnosticForm)
    con ProcessCmdKey para atajos Holyrics (flechas, Espacio, Esc, B/C/L,
    G, F5, F8).
  - IpcClient con NamedPipeClientStream al pipe FusionHP-ipc.
  - Modelos de datos en Data/ (AhpProgram, Scenario, Element, Theme).
  - Arnés de tests propio en FusionHP.Managed.Tests (sin vstest).
- Escrito el Qt shell (src/qt-shell/): CMakeLists.txt con find_package
  Qt5 5.15.2, MainWindow con QMainWindow, botones Estudio/Presentar/
  Biblioteca (mismo IPC que la carcasa gestionada).
- Escritos los tests nativos (tests/native/): arnés doctest con
  test_ahp_format.cpp (4 casos) y test_importers.cpp (4 casos).
- Descargada y consolidada la biblia RVR1909 (dominio público) desde
  aruljohn/Reina-Valera: 66 libros, 31 099 versículos, 6.6 MB JSON.
- Script scripts/consolidar_rvr1909.py para regenerar la biblia.

Stage Summary:
- **101 archivos** en el repo, organizados en monorepo.
- **1 biblia** completa incluida (RVR1909, dominio público).
- **3 biblias** pendientes (RVG libre distribución, RV1960 y NVI
  copyright) — documentado cómo obtenerlas.
- **NO verificado en compilación**: este sandbox es Linux, no hay MSVC
  ni .NET Framework ni Qt 5.15.2. La verificación debe hacerla una
  máquina Windows con el toolchain descrito en docs/agent/
  build_environment.md.
- **NO hay capturas reales** por la misma razón.
- El código está estructurado para compilar dual en Windows; los stubs
  están claramente marcados con TODO(P0) para los próximos commits.
- **Restricciones cumplidas estrictamente**: sin .NET Core, sin WebView,
  sin escrituras al Registro (solo lectura en launcher), sin
  comunicación de red (solo IPC local), sin .cmd/.bat en el producto.

Próximos pasos (piezas siguientes):
1. Reemplazar placeholders de terceros (sqlite3.c/.h, json.hpp, doctest.h,
   spdlog, wil) por las versiones reales en máquina Windows.
2. Implementar el parser de cita bíblica "Salmo 100:4" / "Juan 3:16-18"
   en BibleDatabase::ObtenerCita.
3. Implementar el importador JSON de biblias completo (JsonBible.cpp
   tiene el esqueleto; falta INSERT en BD).
4. Implementar el importador Holyrics JSON completo (HolyricsJson.cpp).
5. Implementar el renderer Direct2D con DWrite y WIC para imágenes.
6. Implementar el handler IPC en el núcleo que despacha comandos al
   Engine (mapeo type → método del Engine).
7. Conectar la carcasa gestionada MainForm → IPC real (ahora muestra
   datos de ejemplo).
8. Generar los .fdb desde los JSON con el CLI --importar-biblia.
