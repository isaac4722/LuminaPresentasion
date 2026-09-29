# Investigación de componentes, librerías, dependencias y recursos — FUSION-HP

Documento vivo: audita lo que el proyecto usa HOY, registra lo evaluado con
su veredicto, y fija las reglas para aceptar nuevas dependencias. Cualquier
nueva pieza que necesite una librería debe consultar este documento y
añadir aquí el análisis antes de integrarla.

Reglas de oro del producto (no negociables, condicionan TODO):
100 % offline · sin Registro de Windows · todo vendido o estático
(ninguna descarga en tiempo de instalación/ejecución) · licencia compatible
con distribución cerrada · auditables los binarios incluidos.

## 1. En uso hoy (verificado en el código)

### Núcleo C++17 (`src/core/third_party/`)

| Componente     | Versión     | Licencia       | Rol real en el proyecto                                    |
|----------------|-------------|----------------|------------------------------------------------------------|
| SQLite         | 3.x amalg   | Dominio público| `cancionero.fdb` + 4 biblias `.fdb`; FTS5 habilitado para búsqueda de texto completo. Compilado con gcc/MSVC /MT. |
| nlohmann/json  | 3.x         | MIT            | `ahp.v1`, protocolo `ipc.v1`, temas, importadores JSON.    |
| pugixml        | 1.14        | MIT            | Lector PPTX directo (ISO/IEC-29500: ZIP+XML) y Zefania XML.|
| stb_image      | 2.30        | Dominio público| Decodificación de imágenes internas del PPTX (JPEG/PNG) y fondos. |
| doctest        | 2.x         | MIT            | Arnés de pruebas nativo (168 casos / 1.281 aserciones).    |

### Sistema operativo (sin binario incluido: API de Windows)

| Componente                | Uso real                                                      |
|---------------------------|---------------------------------------------------------------|
| Direct2D / DirectWrite    | Renderizado de pantalla y rasterizador del modo directo.      |
| WIC                       | Decodificación/encodación de imágenes en el shell.            |
| Media Foundation          | Reproducción de vídeo (VideoPlayer).                          |
| Pipes con nombre          | IPC `ipc.v1` (transporte JSON una línea por mensaje).         |
| SQLite integrado          | No hay DLL externa: el amalgama va enlazado estático (/MT).   |

### Shell y capa gestionada

| Componente     | Rol                                                                  |
|----------------|----------------------------------------------------------------------|
| Qt 5.15.2      | Shell de escritorio alternativo (windeployqt + CRT app-local en el paquete). |
| .NET 4.8       | Consola del operador (WinForms). Solo referencias del framework: SIN NuGet. |
| Inno Setup 6   | Instaladores (solo herramienta de build, no va en el producto).      |

### Lección aprendida de esta sesión

La lista de licencias de terceros anunciaba librerías que el código ya no
usa (Newtonsoft.Json, NLog, PdfSharp, Ookii, wil, spdlog). Consecuencia:
`THIRD_PARTY_LICENSES.txt` se ha alineado con la tabla anterior. Regla: el
documento de licencias solo puede listar lo que está vendido o referenciado
de verdad (el gate de calidad lo comprueba).

## 2. Evaluadas con veredicto (no integrar sin revisar aquí)

| Candidata       | Para qué serviría                          | Veredicto | Razón |
|-----------------|---------------------------------------------|-----------|-------|
| miniz           | Escritor ZIP con deflate (exportar .pptx más pequeño) | **ACEPTADA como candidata P2** | Un solo archivo .c/.h, dominio público; el exportador actual produce entradas ALMACENADAS (OPC válido pero pesado). Integrar SOLO si el tamaño del export importa. |
| zlib            | Compresión general                          | Rechazada | miniz la supera para nuestro único caso; menos piezas vendidas. |
| expat           | Parser XML SAX                              | Rechazada | pugixml ya cubre DOM y SAX; duplicar parsers es deuda de mantenimiento. |
| FreeType        | Renderizado de fuentes propio               | Rechazada | DirectWrite ya lo hace con hinting del sistema y menos memoria. |
| FFmpeg          | Vídeo/audio                                 | Rechazada | 100+ MB, licencia LGPL problemática en binario estático; Media Foundation cubre MP4/H.264 nativo de Windows. |
| WebView2        | UI web embebida                             | Rechazada | Descarga runtime en instalación → rompe la regla 100 % offline. |
| BouncyCastle (C#) | Descifrado Twofish de e-Sword (.bib)      | **ACEPTADA como candidata P2** | Solo DLL firmada vendida (no descarga); desbloquearía el importador e-Sword hoy stub. |
| libcurl         | Red                                         | Rechazada | El producto no tiene red por diseño. |
| Boost           | Utilidades generales                        | Rechazada | El estándar C++17 + los vendidos actuales cubren todo; peso de build injustificado. |

## 3. Recursos de datos (biblias, cantos, arte)

| Recurso                  | Estado     | Notas |
|--------------------------|------------|-------|
| Biblia RVR1909           | Embebida   | Se siembra al primer arranque en UNA transacción (2,5 s para 31.099 versículos). |
| Formatos de biblia       | JSON interno, TSV, Zefania XML activos; e-Sword stub | Añadir MySword (.sqlite) sería barato: es SQLite plano y ya llevamos SQLite. |
| Formatos de canto        | Holyrics JSON activo | OpenSong XML y SoftProjector son candidatos de bajo coste con pugixml. |
| Fuentes tipográficas     | Sistema    | Segoe UI/GUI del SO; no empaquetar fuentes de terceros sin licencia revisada. |
| Logo de reposo / assets  | `data/assets` | El operador puede sustituirlos sin recompilar. |

## 4. Reglas para ACEPTAR una dependencia nueva

1. Offline total: cero descargas en build de usuario o runtime.
2. Vendida en `src/core/third_party/` (o DLL firmada en el paquete) y
   enlazada estática cuando sea C/C++ (/MT, sin CRT dinámico extra).
3. Licencia MIT/BSD/dominio público/Apache-2.0; copyleft fuerte NO.
4. Registrada en este documento y en `THIRD_PARTY_LICENSES.txt` el mismo
   commit que la integra.
5. Probada: al menos un test del arnés que la ejerza (los vendidos ya
   tienen cobertura: sqlite en tests de BD, pugixml en lector PPTX,
   nlohmann en despachador y ahp).
6. Justificada: si el estándar o lo ya vendido lo cubre, NO se añade.
