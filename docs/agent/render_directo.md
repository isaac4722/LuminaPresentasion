# Render directo (modo "directo", 9.2 → pantalla y exportación)

De `DiapositivaPptx` (leída por `LectorPptx`, ver `pptx_directo.md`) +
tema resuelto (ver `themes.md`) a píxeles, sin PowerPoint. Esta pieza
cierra el P1 "rasterizador del modo directo" de AGENT.md y de paso
convierte `RendererDirect2D` de stub a implementación real.

## 1. Arquitectura en dos mitades

```
LectorPptx ──► DiapositivaPptx ──┐
                                 ├─► RenderDirecto::ConstruirPlan (PORTABLE,
HerenciaTemas ─► ResolucionTema ─┘   testado en Linux/MSVC)
                                        │
                                        ▼
                                 PlanRenderDirecto (pasos con rect 0..1)
                                        │
              ┌─────────────────────────┴──────────────────────────┐
              ▼                                                    ▼
   DibujarPlan(Renderer*)                              RasterizadorDirectoD2D
   → RendererDirect2D (HWND, proyección)               (WIC offscreen → RGBA,
   → RendererGrabador (tests)                           exportación 1080p)
              └────────────── ambas usan rendirecto::*EnRT ─────────────┘
```

**Una sola fuente de verdad del pintado**: `RenderDirectoInterno.h`
(`PintarFondoEnRT`, `DibujarPasoEnRT`, `DibujarImagenEnRT`) lo consumen
TANTO el renderer de pantalla como el rasterizador offscreen. Proyección
y exportación jamás divergen.

## 2. Plan portable (`RenderDirecto::ConstruirPlan`)

Layout con bandas normalizadas (0..1), independientes de resolución:

| Banda    | x     | y     | w     | h     |
|----------|-------|-------|-------|-------|
| Título   | 0.08  | 0.10  | 0.84  | 0.18  |
| Cuerpo (con título)  | 0.08 | 0.34 | 0.84 | 0.56 |
| Cuerpo (sin título)  | 0.08 | 0.10 | 0.84 | 0.80 |

- Cada párrafo ocupa un **slot vertical igual** dentro de la banda; el
  texto queda centrado verticalmente en su slot (DirectWrite
  `PARAGRAPH_ALIGNMENT_CENTER`).
- `aspecto` (de `InfoPptx`: `ancho_emu/alto_emu`) viaja en el plan
  (default 16:9 si <=0, clamp defensivo [0.5, 4.0]); es informativo para
  el shell (letterbox) — el v1 pinta full-bleed.
- El tema resuelto se aplica con `AplicarAEstilos`: texto y fondo del
  tema activo (5 niveles de herencia incluidos). Valores inválidos →
  aviso y se conserva el default. **Fondo tipo imagen → aviso y se usa
  `fondo.color1`** (fondo de imagen en modo directo: pieza siguiente).
- `max_parrafos` (12 por defecto): el excedente se descarta **con
  aviso**, nada silencioso.
- Diapositiva vacía → solo fondo (negro sólido si el tema no define;
  la salida nunca muestra el escritorio, doc 6.1).
- `Utf8AUtf16`: decodificador propio (sin API de Windows). U+FFFD por
  subparte máxima según Unicode; el byte ofensor NO se traga (resync);
  truncado al final → U+FFFD y termina. Soporta 2/3/4 bytes, pares
  suplentes (emoji), detecta sobrecodificación y suplentes UTF-8.

## 3. D2D compartido (Windows)

- `RendererDirect2D::DibujarTexto` ya es real: `IDWriteTextFormat` con
  cache por clave (familia|tamaño|negrita|cursiva|alineación), wrap,
  color y centrado del tema. `DibujarFondo` soporta sólido, **gradiente
  vertical** e imagen (WIC + cubrir/contener/estirar con clip).
- `RasterizadorDirectoD2D::Rasterizar`: bitmap WIC PBGRA + render target
  SOFTWARE → `EndDraw` → lock → PBGRA premultiplicado → RGBA recto
  (conversión general des-premultiplicando; el caso alfa 255 solo
  permuta B↔R). Límites: objetivo [1, 8192] px por lado, error
  explícito si D2D falla (hr en el mensaje).
- El shell consume `Rasterizar` para: pre-render de proyección del modo
  directo y el seam `RasterizadorUnidad` de `Exportador::ExportarImagenes`
  (1920x1080).

## 4. Fuera de alcance v1 (piezas siguientes)

- Imágenes/formas internas de las diapositivas pptx (posición EMU,
  media del paquete `ppt/media/`).
- Estilos por-run del pptx (negrita/tamaño/color por run).
- Letterbox automático cuando el monitor no coincide con `aspecto`.
- Fondo tipo imagen en el modo directo (hoy: aviso + color1).

## 5. Pruebas

- `tests/native/test_render_directo.cpp` (portable, gcc-14 y MSVC):
  Utf8AUtf16 (ASCII/acentos/CJK/emoji/suplentes/roto/truncado/suelto/
  sobrecodificado), bandas y slots del plan, truncado de párrafos con
  aviso, tema vacío/inválido/imagen, clamp de aspecto, despacho
  `DibujarPlan` con mock de grabación (orden y escalado), no-ops
  seguros.
- La parte D2D (`RasterizadorDirectoD2D`, rutinas `*EnRT`) solo compila
  en MSVC (CI: núcleo x86/x64 + qt_shell).
