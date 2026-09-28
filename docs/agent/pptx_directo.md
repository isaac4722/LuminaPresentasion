# Lector directo de paquetes PPTX (`PptxDirecto.h`)

Sección 9.2 del doc técnico — modo `"directo"`: proyectar el archivo
original **sin PowerPoint** con el render propio del núcleo. Esta pieza
es el lector portable de paquetes `.pptx`/`.pptm` (ISO/IEC-29500); el
shell Windows la consume para rasterizar cada diapositiva.

## Reglas del producto que aquí se aplican

- **Leer no ejecuta nada.** Un `.pptm` se lee igual que un `.pptx`; las
  macros jamás se ejecutan (regla del producto, refuerza a PlanPptx).
- **Dependencias vendidas y auditables.** El lector ZIP y el inflador
  RFC 1951 siguen siendo propios (verificados con paquete embebido y
  CRC). El parseo XML usa **pugixml 1.14** (MIT, vendida en
  `third_party/pugixml`, sin excepciones/STL/XPath) y la media del
  paquete se decodifica con **stb_image 2.30** (dominio público, vendida
  en `third_party/stb`; PNG/JPEG/BMP/GIF → RGBA, límite 64 Mp por
  imagen). Nada se descarga en runtime (regla 100% offline).
- **Errores explícitos, sin retroceso silencioso**: paquete no-zip,
  directorio central roto, CRC32 que no coincide, deflate corrupto,
  sin `officeDocument`, parte de presentación ausente → `ok=false` con
  `msg_error` en español.
- **Fidelidad con el archivo**: el orden de diapositivas es el de
  `p:sldIdLst` (no el orden de nombres de partes). Si una parte de
  diapositiva falta o está rota, se omite con aviso y el índice de las
  restantes conserva la posición original de la lista.

## Qué extrae (v2)

Por paquete: `p:sldSz` en EMU (`ancho_emu`/`alto_emu`, 914400 EMU = 1")
para la relación de aspecto del render.

Por diapositiva, en orden de presentación:

| Campo            | Origen |
|------------------|--------|
| `titulo`         | Placeholder `type="title"`/`"ctrTitle"` (runs unidos con espacio) |
| `parrafos`       | Texto plano de cada párrafo rico (compatibilidad v1) |
| `parrafos_ricos` | `a:p` → `a:r` con `a:rPr`: `sz` (centésimas de punto → `tam_pt`), `b`, `i`, `u`, color `a:solidFill/a:srgbClr@val` → `#RRGGBB` |
| `imagenes`       | `p:pic`: `a:xfrm` (off/ext EMU) + `a:blip@r:embed` → relaciones de la diapositiva → parte media decodificada a RGBA (`ancho`/`alto`/`rgba`, `parte` = ruta en el paquete) |

- Los atributos de relación (`r:id`, `r:embed`) se buscan **prefiriendo
  el namespaced**: `p:sldId` lleva también un `id` plano (número interno)
  que NO es la relación (bug real hallado en verificación).
- Los nodos se comparan por **nombre local** (`p:sp` ≡ `<x:sp>`): los
  prefijos legales distintos de p:/a:/r: se aceptan.
- La tabla vive en `p:graphic/a:graphicData/a:tbl` (búsqueda en
  profundidad acotada al marco); `p:ph` se busca en profundidad dentro
  de `p:nvSpPr`.
- Avisos explícitos (nada silencioso): `p:pic` sin `r:embed`,
  relación inexistente, media ausente, media no decodificable, XML de
  diapositiva roto.
- Límite antizip-bomb del lector ZIP intacto; la media decodificada se
  acota a 64 Mp (256 MiB de RGBA) por imagen.
- Leer no ejecuta nada (regla `.pptm` intacta).

## Qué extraía (v1, histórico)

Por paquete: `p:sldSz` en EMU (`ancho_emu`/`alto_emu`, 914400 EMU = 1")
para la relación de aspecto del render.

Por diapositiva, en orden de presentación:

| Campo     | Origen                                                          |
|-----------|-----------------------------------------------------------------|
| `titulo`  | Placeholder `type="title"` o `"ctrTitle"` (primer shape que lo declare; párrafos unidos con espacio) |
| `parrafos`| Todos los `a:p` → `a:t` en orden: shapes, cuadros de texto, tablas (`a:tbl` dentro de `p:graphicFrame`) |

- Los runs (`a:r`) de un párrafo se unen **sin separador** (son
  fragmentos del mismo párrafo).
- Entidades XML decodificadas: `&lt; &gt; &amp; &quot; &apos;`,
  `&#NN;` y `&#xHHHH;` (UTF-8 manual).
- `subTitle` y demás placeholders **no** son título (van al cuerpo).

## Fuera de alcance v1 (siguiente pieza del render directo)

- Imágenes y formas vectoriales dentro de las diapositivas
  (posicionamiento EMU, media del paquete `ppt/media/`).
- Estilos por-run (negritas, tamaños, color por run): la proyección
  "directa" v1 pinta el texto con el tema activo del escenario.

## Límites defensivos

- Entrada de deflate limitada a 256 MiB descomprimidos (antizip-bomb).
- Todas las lecturas ZIP están acotadas (`pos + n <= tam`).
- ZIP64 se soporta de forma defensiva (EOCD, localizador, campos extra
  por entrada) aunque PowerPoint no lo emita para presentaciones
  normales.
- El CRC32 de cada entrada se verifica siempre.

## Pruebas

`tests/native/test_pptx_directo.cpp` + paquete embebido
`tests/native/pptx_muestra.h` (generado por
`scripts/gen_pptx_muestra.py`, deflate real de zlib, NO editar a mano):
contenido exacto por diapositiva, entidades, runs unidos, orden,
`sldSz`, entradas almacenadas, comentario EOCD, diapositiva ausente
(índice preservado), zip roto/truncado, CRC corrupto, sin
officeDocument, presentación ausente y coherencia `PlanPptx` ↔
`LectorPptx::EsPaquetePptx`.
