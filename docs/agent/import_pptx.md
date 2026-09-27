# Importador PPTX (OpenXML / OPC) — capa C#

Sección 9.2 del doc técnico. Conforme al doc ("todos los
importadores/exportadores viven en la capa C#"), este importador vive en
`src/managed/FusionHP.Managed/Interop/ImportadorPptx.cs` y convierte un
`.pptx`/`.pptm` en un `AhpProgram` (ahp.v1) + informe de importación.

## Recorrido normativo del paquete (doc 9.2.1)

`[Content_Types].xml` → `_rels/.rels` (Type `/officeDocument`) →
`docProps/core.xml` (título/autor) → `ppt/presentation.xml`
(`sldIdLst` = orden real) → relaciones → `ppt/slides/slideN.xml` →
layout → master (solo para el fondo).

## Seguridad (doc 9.2.4)

- `XmlReaderSettings.DtdProcessing = Prohibit` y `XmlResolver = null`:
  sin DTD, sin entidades externas (XXE), sin expansión recursiva
  ("billion laughs"). Un paquete con DTD se rechaza con error explícito.
- `.pptm`: se importa SIN ejecutar macros, siempre, con aviso explícito
  en el informe (`InformeImportacionPptx.TieneMacros`).

## Tolerancia a namespaces (doc 9.2.5)

Todo el matching se hace por `LocalName` (ignora prefijos y URIs), así
los namespaces de extensión (`p14`, `pc2`, ...) y los elementos
desconocidos se ignoran y jamás abortan la importación.

## Mapeo al modelo propio (doc 9.2.6)

| PPTX                       | ahp.v1                                             |
|----------------------------|----------------------------------------------------|
| diapositiva                | Escenario (`scn-import-N`, determinista)           |
| placeholder title/ctrTitle | `Escenario.Nombre` (párrafos unidos con espacio)   |
| caja de texto / subTitle   | Elemento `texto` (un párrafo = una línea)          |
| runs con `sz` uniforme     | `tam_fuente_pt` = sz/100 (doc 9.2.3); mezclado → 0 (hereda) |
| imagen `p:pic` incrustada  | Elemento `imagen` + extracción a `media/` (el llamador persiste `MediaExtraida`) |
| tabla `a:tbl`              | Elemento `texto` (celdas → líneas, fila por fila)  |
| fondo sólido slide→layout→master | `Escenario.FondoColor` (`#RRGGBB` mayúsculas) |
| fondo con imagen           | omitido (v1), registrado en el informe             |
| animaciones/transiciones   | omitidas (MVP), el doc lo exige                    |
| coordenadas EMU            | no importadas en v1 (sin geometría de lienzo en ahp.v1); decisión en el informe |

## Informe de importación (doc 9.2.6 / 9.3.4)

`InformeImportacionPptx` separa `Avisos` (no bloquean: macros, paquete
sin diapositivas) y `Omitidos` (decisiones de fidelidad: fondo con
imagen, imagen sin relación, animaciones). La importación solo falla
con error explícito (`MsgError`) cuando el paquete no es OPC, no
declara `officeDocument` o falta la parte de presentación.

## Pruebas

- C# (`ImportadorPptxTests.cs`): 9 casos contra el MISMO paquete de
  prueba que el núcleo C++ (base64 regenerado por
  `scripts/gen_pptx_muestra_cs.py` desde `tests/native/pptx_muestra.h`).
  Se ejecutan en CI (job net48) con el arnés propio.
- Nota: no hay dotnet/mono en el entorno local de desarrollo; la
  verificación del importador es 100% CI (MSBuild net48 + runner).

## Rendimiento

Lineal en el tamaño del paquete (lectura única, sin reentradas); el
objetivo del doc 10.1 (100 diapositivas con texto+imágenes ≤ 10 s en
perfil A) es alcanzable; medición real pendiente de la ventana de
diagnóstico (11.3).
