# Lector directo de paquetes PPTX (`PptxDirecto.h`)

Sección 9.2 del doc técnico — modo `"directo"`: proyectar el archivo
original **sin PowerPoint** con el render propio del núcleo. Esta pieza
es el lector portable de paquetes `.pptx`/`.pptm` (ISO/IEC-29500); el
shell Windows la consume para rasterizar cada diapositiva.

## Reglas del producto que aquí se aplican

- **Leer no ejecuta nada.** Un `.pptm` se lee igual que un `.pptx`; las
  macros jamás se ejecutan (regla del producto, refuerza a PlanPptx).
- **Sin dependencias nuevas.** El lector ZIP, el inflador RFC 1951 y el
  mini-extractor XML son propios (~900 líneas). Nada que auditar de
  terceros ni licencias que añadir.
- **Errores explícitos, sin retroceso silencioso**: paquete no-zip,
  directorio central roto, CRC32 que no coincide, deflate corrupto,
  sin `officeDocument`, parte de presentación ausente → `ok=false` con
  `msg_error` en español.
- **Fidelidad con el archivo**: el orden de diapositivas es el de
  `p:sldIdLst` (no el orden de nombres de partes). Si una parte de
  diapositiva falta o está rota, se omite con aviso y el índice de las
  restantes conserva la posición original de la lista.

## Qué extrae (v1)

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
