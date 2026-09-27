# data/bibles/

Esta carpeta contiene las biblias preinstaladas.

## Formato

Cada biblia está en **JSON interno de FUSION-HP** (ver
`data/samples/bible_sample.json`). El launcher / el núcleo importan el JSON
a un `.fdb` (SQLite) en el primer arranque. Ambos formatos son válidos como
fuente; el `.fdb` generado es el que usa el núcleo en caliente.

## Biblias incluidas en este commit

| Archivo           | Nombre                  | Abrev    | Origen                | Licencia          | Versículos |
|-------------------|-------------------------|----------|-----------------------|-------------------|-----------:|
| `RVR1909.json`    | Reina-Valera 1909       | RVR1909  | aruljohn/Reina-Valera | Dominio público  |  31 099    |

## Biblias pendientes

| Archivo           | Nombre                  | Abrev    | Cómo obtener                                          |
|-------------------|-------------------------|----------|-------------------------------------------------------|
| `RVG.json`        | Reina-Valera Gómez 2010 | RVG      | https://www.rvgespanol.com/descargas (XML Zefania)   |
| `RV1960.json`     | Reina-Valera 1960       | RV1960   | Proveer `.bib` (e-Sword) propio; copyright SBU       |
| `NVI.json`        | Nueva Versión Internac.| NVI      | Proveer `.xml` (Zefania) propio; copyright Bílica    |

Las dos últimas están sujetas a copyright de sus editores. **No se pueden
redistribuir sin licencia**. La aplicación funciona con las biblias que el
usuario ponga aquí; si faltan, la pantalla de Biblioteca lo avisa.

## Cómo regenerar RVR1909.json

El script `scripts/consolidar_rvr1909.py` toma el repo
`aruljohn/Reina-Valera` (66 archivos JSON uno por libro) y los consolida
en este único archivo JSON.

```powershell
# En Windows (o Linux/macOS con Python 3.9+):
git clone https://github.com/aruljohn/Reina-Valera /tmp/Reina-Valera
python scripts/consolidar_rvr1909.py /tmp/Reina-Valera data/bibles/RVR1909.json
```

## Importación a .fdb (en el primer arranque)

El launcher del producto verifica la presencia de los `.fdb`. Si falta y
existe el `.json`, lo importa automáticamente con aviso al usuario (una
sola vez, marcado en `runtime\bibles_installed.flag`).

Comando equivalente desde CLI:

```
FusionCore.exe --importar-biblia --formato json --entrada data/bibles/RVR1909.json --salida data/bibles/RVR1909.fdb
```

## Verificación de integridad

El importador valida:

- 66 libros presentes (esquema en `data/schema/bible.sql`).
- Versículos numerados secuencialmente dentro de cada capítulo.
- Capítulos numerados secuencialmente dentro de cada libro.
- Total de versículos cercano a 31 102 ± 50 (variaciones por
  inclusión/exclusión de subtítulos y "amén" final).

Para RVR1909.json generado el 2026-09-27: **66 libros, 31 099 versículos**.
