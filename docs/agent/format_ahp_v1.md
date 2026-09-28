# Formato `ahp.v1` — Programas de culto

> **Versión**: `ahp.v1`
> **Tipo**: JSON UTF-8 con BOM opcional
> **Extensión**: `.ahp`
> **Compatibilidad hacia adelante**: el parser ignora claves desconocidas y
> avisa por log sin fallar.

## Resumen

Un programa de culto es una colección ordenada de **escenarios**. Cada
escenario contiene **elementos** (texto, versículo, imagen, vídeo, lower
third). El programa tiene metadatos (título, fecha, tema raíz) y un
historial de uso.

## Esquema JSON

```json
{
  "$schema": "https://fusionhp.local/schemas/ahp.v1.json",
  "formato": "ahp",
  "version": 1,
  "meta": {
    "titulo": "Culto Domingo 2026-09-28",
    "fecha": "2026-09-28",
    "autor": "Iglesia Ejemplo",
    "tema_raiz": "Dominical",
    "notas": "Servicio de adoración"
  },
  "recientes": [
    { "ruta": "data/programas/culto-dom.ahp", "veces_usado": 12 }
  ],
  "escenarios": [
    {
      "id": "esc-001",
      "nombre": "Bienvenida",
      "notas": "Anuncio inicial",
      "tema": "DominicalCálido",
      "elementos": [
        {
          "id": "el-001",
          "tipo": "texto",
          "titulo": "Bienvenida",
          "lineas": [
            { "texto": "Bienvenidos a la Casa del Señor", "marca": "1" },
            { "texto": "Hoy es día de adoración", "marca": "2" }
          ],
          "tema_override": null,
          "notas": "Subir volumen al iniciar"
        },
        {
          "id": "el-002",
          "tipo": "versiculo",
          "cita": "Salmo 100:4",
          "biblia": "RVR1909",
          "texto": "Entrad por sus puertas con acción de gracias...",
          "modo": "completo"
        },
        {
          "id": "el-003",
          "tipo": "imagen",
          "ruta": "data/medios/portada.png"
        }
      ]
    }
  ],
  "historial_uso": [
    { "fecha": "2026-09-21T10:00:00-04:00", "operador": "josue" }
  ]
}
```

## Escenario: campo `fondo`

Color sólido del escenario (`"#RRGGBB"` o `"#RGB"`) o vacío/ausente =
hereda del tema. Es el destino del "fondo del diseño" al importar un
PPTX (doc técnico 9.2.6: el fondo del diseño se traduce a fondo del
Escenario). `Validar()` rechaza valores que no sean un color hex válido.

## Escenario: campo `tema_escenario`

Capa inline del nivel Escenario de la herencia (doc técnico 5.4, tercer
eslabón de la cascada Tema → Plantilla → Escenario → Elemento). Objeto
de claves planas `string → string` con el mismo esquema que un tema
(`texto.color`, `fondo.tipo`, ...; ver `themes.md`); `null` o ausente =
capa vacía. Semántica al resolver:

- Se aplica **encima** de la plantilla (`escenario.tema`) y **debajo**
  del override del elemento: lo que el operador fija aquí gana a la
  plantilla para TODO el escenario.
- El campo `fondo` se pliega en esta misma capa como
  `fondo.tipo=solido` + `fondo.color1`; si el bolso inline define
  `fondo.tipo` o `fondo.color1`, **gana el bolso** (lo explícito manda).
- Reglas de parseo estricto: no-objeto o propiedad no-string → error
  explícito de carga (no silencioso). `Validar()` comprueba en frío que
  `texto.color`, `fondo.color1` y `fondo.color2` sean hex válidos.
- Claves desconocidas se conservan (tolerancia hacia adelante) y entran
  al informe de fidelidad si cambian algo.

```json
{
  "id": "esc-002",
  "nombre": "Oración",
  "tema": "DominicalCálido",
  "fondo": "#101828",
  "tema_escenario": { "texto.color": "#DDDDDD", "texto.tamano": "72" },
  "elementos": []
}
```

## Tipos de elemento

### `texto`

Letras de cantos, anuncios, oraciones. Cada línea puede tener `marca` (una
etiqueta de sincronización que se usa para saltar línea por línea).

| Campo         | Tipo             | Requerido | Notas                                  |
|---------------|------------------|-----------|----------------------------------------|
| `tipo`        | `"texto"`        | sí        |                                        |
| `titulo`      | string           | sí        |                                        |
| `lineas`      | array de objetos | sí        | `[{texto, marca?}]`                    |
| `acordes`     | string?          | no        | Texto con acordes estilo ChordPro      |
| `tono_origen` | string?          | no        | p.ej. `"C"`                            |
| `tono_actual` | string?          | no        | p.ej. `"D"` (si se transpuso)          |
| `bpm`         | number?          | no        |                                        |
| `tam_fuente_pt` | number?        | no        | Tamaño uniforme de la caja en puntos (sz/100 de PresentationML al importar PPTX, doc 9.2.3); 0/ausente = hereda del tema |

### `versiculo`

Cita bíblica. El texto se carga desde la BD en runtime, pero se cachea en
el `.ahp` para que el archivo sea autocontenido si la biblia no está
instalada.

| Campo    | Tipo                      | Requerido | Notas                                |
|----------|---------------------------|-----------|--------------------------------------|
| `tipo`   | `"versiculo"`             | sí        |                                      |
| `cita`   | string                    | sí        | `"Salmo 100:4"` o `"Juan 3:16-18"`  |
| `biblia` | string                    | sí        | `RVR1909`, `RV1960`, `NVI`, `RVG`    |
| `texto`  | string                    | sí        | snapshot cacheado (autocontenido)    |
| `modo`   | `"completo"` / `"tercio"` | no        | `tercio` = lower third              |

### `imagen`

| Campo   | Tipo      | Requerido | Notas                          |
|---------|-----------|-----------|--------------------------------|
| `tipo`  | `"imagen"` | sí        |                                |
| `ruta`  | string    | sí        | Ruta relativa al programa o absoluta |
| `ajuste`| `"cubrir"` / `"contener"` / `"estirar"` | no | Default: `cubrir` |

### `video`

| Campo     | Tipo      | Requerido | Notas                              |
|-----------|-----------|-----------|------------------------------------|
| `tipo`    | `"video"` | sí        |                                    |
| `ruta`    | string    | sí        |                                    |
| `bucle`   | boolean   | no        | Default: `false`                   |
| `audio`   | boolean   | no        | Default: `true`                    |

### `lower_third`

| Campo    | Tipo    | Requerido | Notas                          |
|----------|---------|-----------|--------------------------------|
| `tipo`   | `"lower_third"` | sí        |                          |
| `titulo` | string  | sí        | Texto principal                |
| `sub`    | string? | no        | Texto secundario (ej. cita)    |
| `color`  | string? | no        | Hex, default del tema          |

### `pptx` (P1)

Proyecta un archivo PPTX original. Modos: `com` (con PowerPoint) o
`directo` (sin PowerPoint, render propio).

| Campo    | Tipo                  | Requerido | Notas                       |
|----------|-----------------------|-----------|-----------------------------|
| `tipo`   | `"pptx"`              | sí        |                             |
| `ruta`   | string                | sí        |                             |
| `modo`   | `"com"` / `"directo"` | sí        |                             |

Notas de compatibilidad: si `modo` falta, el parser asume `"com"`
(archivos antiguos); un valor no reconocido se lee como inválido y
`Validar()` lo rechaza con error explícito. Las macros de un `.pptm`
nunca se ejecutan. La planificación de la proyección vive en
`fusion::core::PlanPptx` y la de las exportaciones (PPTX/PDF/1080p) en
`fusion::core::PlanExport` — ambas portable, con tests en
`tests/native/test_planes.cpp`.

## Temas y herencia

La herencia de temas es de **4 niveles**:

1. `Tema` raíz del programa (`meta.tema_raiz`).
2. `Plantilla` del escenario (`escenario.tema`).
3. `Tema override` del elemento (`elemento.tema_override`).
4. `Tema runtime` aplicado en caliente por el operador.

Cada nivel puede sobreescribir selectivamente propiedades del anterior. El
**informe de fidelidad** (P2) enumera qué propiedades fueron sobreescritas en
cada nivel, para que el operador entienda qué se le aplicó al elemento final.

Ver `docs/agent/themes.md` para el esquema completo de un tema y el
algoritmo de resolución (implementado en
`src/core/src/core/HerenciaTemas.cpp`, tests en
`tests/native/test_herencia.cpp`).

## Recientes

La lista `recientes` contiene programas abiertos antes (no el actual). Se
usa para llenar la pantalla de Inicio. Persistente en `runtime/recientes.json`.

## Validación

El parser `fusion::data::AhpFormat::Cargar()` debe:

1. Rechazar archivos con `version > 1` con error explícito (no silencioso).
2. Tolerar claves desconocidas (ignorar + log `warning`).
3. Validar tipos de elemento (`tipo` ∈ {texto, versiculo, imagen, video,
   lower_third, pptx}).
4. Validar que cada elemento tenga `id` único dentro de su escenario.
5. Validar que cada escenario tenga `id` único dentro del programa.

Tests del arnés nativo: `tests/native/test_ahp_format.cpp`.
Tests del arnés gestionado: `tests/managed/AhpFormatTests.cs`.
