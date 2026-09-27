-- ============================================================================
-- cancionero.sql — Esquema SQLite para cancionero.fdb
-- Base de datos única de cantos. Cargada una vez al arrancar el núcleo.
-- Lista completa directa, sin tope ni búsqueda obligatoria.
-- ============================================================================

PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;
PRAGMA encoding = 'UTF-8';

-- ---------------------------------------------------------------------------
-- Tabla principal de cantos
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS cantos (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    titulo          TEXT    NOT NULL,
    autor           TEXT,
    coleccion       TEXT,         -- p.ej. "Himnario Bautista", "Rivera 2010"
    tono_origen     TEXT,         -- p.ej. "C", "Am", "F#"
    bpm             INTEGER,
    clave_numerica  TEXT,         -- notación por números (solfeggio)
    idioma          TEXT DEFAULT 'es',  -- ISO 639-1
    derechos        TEXT,         -- texto de copyright
    fuente          TEXT,         -- origen: "Holyrics", "JSON", "manual"
    fecha_import    TEXT NOT NULL DEFAULT (datetime('now')),
    fecha_modif     TEXT NOT NULL DEFAULT (datetime('now')),
    veces_usado     INTEGER NOT NULL DEFAULT 0,
    notas           TEXT
);

CREATE INDEX IF NOT EXISTS idx_cantos_titulo       ON cantos (titulo);
CREATE INDEX IF NOT EXISTS idx_cantos_autor       ON cantos (autor);
CREATE INDEX IF NOT EXISTS idx_cantos_veces_usado ON cantos (veces_usado DESC);

-- ---------------------------------------------------------------------------
-- Secciones (Verso, Coro, Puente, etc.)
-- Un canto tiene una o más secciones, ordenadas.
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS canto_secciones (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    canto_id        INTEGER NOT NULL,
    orden           INTEGER NOT NULL,
    tipo            TEXT    NOT NULL CHECK (tipo IN (
                        'verso','coro','puente','intro','outro','instrumental','tag','estrofa'
                    )),
    etiqueta        TEXT,         -- p.ej. "Verso 1", "Coro", "Puente"
    UNIQUE (canto_id, orden),
    FOREIGN KEY (canto_id) REFERENCES cantos (id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_secciones_canto ON canto_secciones (canto_id, orden);

-- ---------------------------------------------------------------------------
-- Líneas de cada sección, con acordes opcionales (formato ChordPro inline)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS canto_lineas (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    seccion_id      INTEGER NOT NULL,
    orden           INTEGER NOT NULL,
    texto           TEXT    NOT NULL,
    acordes         TEXT,         -- ChordPro inline, p.ej. "[C]Bienvenidos [G]al Señor"
    marca           TEXT,         -- etiqueta de sincronización, opcional
    FOREIGN KEY (seccion_id) REFERENCES canto_secciones (id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_lineas_seccion ON canto_lineas (seccion_id, orden);

-- ---------------------------------------------------------------------------
-- Etiquetas / categorías (N:M con cantos)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS etiquetas (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    nombre          TEXT    NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS canto_etiquetas (
    canto_id        INTEGER NOT NULL,
    etiqueta_id     INTEGER NOT NULL,
    PRIMARY KEY (canto_id, etiqueta_id),
    FOREIGN KEY (canto_id)    REFERENCES cantos    (id) ON DELETE CASCADE,
    FOREIGN KEY (etiqueta_id) REFERENCES etiquetas (id) ON DELETE CASCADE
);

-- ---------------------------------------------------------------------------
-- Favoritos (lista plana del operador)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS favoritos_cantos (
    canto_id        INTEGER PRIMARY KEY,
    fecha           TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (canto_id) REFERENCES cantos (id) ON DELETE CASCADE
);

-- ---------------------------------------------------------------------------
-- Trigger: actualizar fecha_modif al editar
-- ---------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_cantos_modif
    AFTER UPDATE ON cantos
    FOR EACH ROW
    WHEN NEW.fecha_modif = OLD.fecha_modif
BEGIN
    UPDATE cantos SET fecha_modif = datetime('now') WHERE id = NEW.id;
END;

-- ---------------------------------------------------------------------------
-- Vista de búsqueda unificada (texto + acordes)
-- Permite búsqueda LIKE sin tocar tablas múltiples desde la app.
-- ---------------------------------------------------------------------------
CREATE VIEW IF NOT EXISTS v_cantos_busqueda AS
SELECT
    c.id,
    c.titulo,
    c.autor,
    c.tono_origen,
    c.bpm,
    c.veces_usado,
    GROUP_CONCAT(s.etiqueta || ': ' || l.texto, ' || ') AS texto_completo
FROM cantos c
LEFT JOIN canto_secciones s ON s.canto_id = c.id
LEFT JOIN canto_lineas   l ON l.seccion_id = s.id
GROUP BY c.id;

-- ---------------------------------------------------------------------------
-- Metadatos de versión del esquema (para migraciones futuras)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS schema_meta (
    clave       TEXT PRIMARY KEY,
    valor       TEXT NOT NULL
);

INSERT OR IGNORE INTO schema_meta (clave, valor) VALUES ('esquema', 'cancionero');
INSERT OR IGNORE INTO schema_meta (clave, valor) VALUES ('version',  '1');
INSERT OR IGNORE INTO schema_meta (clave, valor) VALUES ('creado',    datetime('now'));
