# Documentación interna del agente — FUSION-HP

Esta carpeta contiene los documentos de referencia que el agente de
desarrollo debe consultar durante el trabajo diario. En caso de conflicto
con `AGENT.md`, este último prevalece.

## Índice

| Documento              | Qué define                                                |
|------------------------|----------------------------------------------------------|
| `architecture.md`      | Diseño técnico del sistema, capas y responsabilidades.   |
| `format_ahp_v1.md`     | Especificación del formato `ahp.v1` (programas de culto).|
| `format_ipc_v1.md`     | Especificación del protocolo IPC `ipc.v1`.               |
| `build_environment.md` | Preparación del toolchain MSVC + .NET + Qt + Inno Setup. |
| `quality_gate.md`      | Reglas del gate de calidad (qué debe estar verde).       |
| `conventions.md`       | Convenciones de código C++/C# y de commit.               |
| `themes.md`            | Herencia de temas 5 niveles + informe de fidelidad.      |
| `pptx_directo.md`      | Lector directo de paquetes PPTX (modo "directo").        |
| `render_directo.md`    | Rasterizador del modo directo (plan portable + D2D).     |
| `import_pptx.md`       | Importador PPTX → ahp.v1 (capa C#).                      |
| `exportadores.md`      | Exportadores reales del núcleo (pptx/pdf/png).           |
| `paquetes.md`          | Portable dual e instaladores en CI.                      |

## Cómo usar estos documentos

1. **Antes de empezar una pieza**: leer `conventions.md` y la sección del
   documento técnico correspondiente a la pieza.
2. **Al tocar formato o IPC**: leer `format_ahp_v1.md` o `format_ipc_v1.md`.
3. **Al preparar una máquina nueva**: seguir `build_environment.md`.
4. **Antes de cada commit**: ejecutar `quality_gate.md`.

## Trazabilidad

Cada spec tiene versión (`ahp.v1`, `ipc.v1`). Cualquier cambio incompatible
debe subir la versión menor (`ahp.v2`) y dejar el parser viejo retrocompatible
durante al menos una release completa.
