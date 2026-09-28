# Exportadores reales del núcleo (Sección 9.3)

Pieza: `fusion::Exportador` (`src/core/include/fusion/core/Exportador.h`).
Los tres formatos del doc técnico 9.3 se ejecutan ahora en el núcleo,
portable y 100 % offline, con cero dependencias nuevas.

## Disparo

La exportación la dispara **SIEMPRE el operador de forma explícita**
(regla del producto). Nada aquí corre solo, en segundo plano ni al
salir. El shell (C# o Qt) solo presenta los botones y las rutas.

## Fuente de verdad de las unidades

`PlanExport::EnumerarUnidades(programa)` devuelve las unidades exactas
(una diapositiva/página/archivo por unidad) y `ContarUnidades` delega
en ella: **el plan y la ejecución jamás discrepen**.

Reglas de enumeración (idénticas a las de proyección):

| Elemento            | Unidades                                  |
|---------------------|-------------------------------------------|
| texto / versículo  | una por línea (sin líneas: 1 con respaldo) |
| imagen / vídeo / lower-third / pptx | 1 (referencia, ver abajo)  |

## Formatos

### PPTX (`ExportarPptx`)
Paquete OPC mínimo **válido** (ISO/IEC-29500): `[Content_Types].xml`,
`_rels/.rels`, `ppt/presentation.xml` (sldSz 16:9), relaciones, máster,
layout `blank`, tema mínimo con clr/font/fmtScheme completos, y una
diapositiva por unidad con un cuadro de texto centrado (título 28 pt
negrita + cuerpo 20 pt). Entradas **almacenadas** (método 0) con CRC32
vía `ZipInterno.h`, que es el mismo módulo del lector: el round-trip
con `LectorPptx` está verificado por tests (y por `zipfile` de Python,
herramienta independiente).

**Incrustación de imágenes**: los elementos `imagen` con extensión
`.png`/`.jpg`/`.jpeg` (case-insensitive) se incrustan en el paquete:
parte `ppt/media/imageN.<ext>` (bytes exactos del archivo), relación
`rId2` de la diapositiva, `p:pic` a pantalla completa (12192000x6858000
EMU, `noChangeAspect`) y `Default` de content-type por extensión usada.
El título del elemento, si lo hay, queda como cuadro de texto en la
banda superior (id 3, sin chocar con el id 2 del pic). Archivos
ilegibles → referencia textual + aviso (nada silencioso). Los vídeos y
paquetes pptx siguen como referencia.

**Fondo del Escenario**: el color sólido `escenario.fondo` (herencia
doc 5.4, destino del "fondo del diseño" del 9.2.6) viaja al paquete
como `p:bg` sólido (`srgbClr` RRGGBB; `#RGB` se expande). Fondos
inválidos se omiten sin error (queda el blanco del máster).

### PDF (`ExportarPdf`)
PDF 1.4 con página 960×540 pt (16:9, el mismo marco que la proyección).
Fuentes base-14 Helvetica / Helvetica-Bold con `/WinAnsiEncoding`: los
acentos españoles van en un byte sin incrustar fuentes (UTF-8 → CP1252;
lo no representable → `?`). Xref calculada byte a byte; líneas
envueltas a ~95 caracteres; el desbordamiento se recorta con aviso.
Numeración de objetos dinámica y determinista (1 catálogo, 2 páginas,
3/4 fuentes; por página: XObject de imagen si lo hay, corriente de
contenido y objeto de página).

**Incrustación de imágenes**: JPEG con paso directo `/DCTDecode` (los
bytes del archivo van tal cual; dimensiones leídas del marcador SOF0..
SOF15, C4/C8/CC excluidos) y PNG decodificado a RGB con
`/FlateDecode` (flujo zlib de bloques almacenados de `ZipInterno.h`).
Colocación contain centrada (`cm`/`Do`). El decodificador PNG propio
(`ImagenesExport.h`) soporta profundidad 8, gris/RGB/RGBA (alfa
aplanada sobre blanco) y filtros 0-4; paleta y Adam7 → no soportados
con aviso explícito. Vídeos/pptx/ilegibles → referencia textual + aviso.

**Fondo del Escenario**: `escenario.fondo` válido pinta un rectángulo
del color a página completa (`rg ... re f`) antes del contenido.

### Imágenes 1080p (`ExportarImagenes` + `CodificarPng`)
`CodificarPng(w, h, rgba)` produce PNG RGBA válido (IHDR/IDAT/IEND con
CRC por chunk; el IDAT es un flujo zlib con **bloques almacenados**,
válido según RFC 1950/1951 y decodificable por cualquier visor).
`ExportarImagenes` pide cada unidad al **rasterizador del shell**:

```cpp
using RasterizadorUnidad = std::function<bool(
    int indice_unidad, int ancho_objetivo, int alto_objetivo,
    int* ancho, int* alto, std::vector<unsigned char>* rgba)>;
```

El núcleo no rasteriza: en Windows el shell conecta este enganche a
Direct2D fuera de pantalla con objetivo 1920×1080; en tests basta un
buffer sintético. Salida: `<nombre_base>-01.png`, `-02.png`, ...

## Medios en v1

Vídeo, imagen y paquetes pptx **no se incrustan** en v1: la
diapositiva/página lleva su referencia textual y el `ResultadoExport`
acumula avisos ("no incrustado, v1"). Nada se pierde en silencio;
la incrustación de imágenes es la pieza siguiente.

## Errores

Todos los métodos devuelven `false` + `msg_error` coherente con el
plan (p. ej. programa vacío). `ExportarImagenes` valida el buffer del
rasterizador (tamaño exacto RGBA) y reporta la unidad exacta que
falló; los archivos ya escritos se declaran en `archivos`.
