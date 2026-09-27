-- ============================================================================
-- bible.sql — Esquema SQLite para una biblia
-- Una base por biblia: data/bibles/RVR1909.fdb, NVI.fdb, RVG.fdb, RV1960.fdb
-- ~31.000 versículos por biblia, ~66 libros.
-- El árbol de 66 libros siempre visible en la UI.
-- ============================================================================

PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;
PRAGMA encoding = 'UTF-8';

-- ---------------------------------------------------------------------------
-- Tabla de libros
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS libros (
    id              INTEGER PRIMARY KEY,           -- 1..66 (orden canónico)
    nombre          TEXT    NOT NULL,              -- "Génesis"
    abrev3          TEXT    NOT NULL,              -- "Gén"
    abrev2          TEXT,                          -- "Gn"  (para citas tipo "Gn 1:1")
    testamento      TEXT    NOT NULL CHECK (testamento IN ('AT','NT')),
    UNIQUE (abrev3)
);

CREATE INDEX IF NOT EXISTS idx_libros_abrev2 ON libros (abrev2);
CREATE INDEX IF NOT EXISTS idx_libros_orden ON libros (id);

-- ---------------------------------------------------------------------------
-- Capítulos
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS capitulos (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    libro_id        INTEGER NOT NULL,
    numero          INTEGER NOT NULL,             -- 1..N
    UNIQUE (libro_id, numero),
    FOREIGN KEY (libro_id) REFERENCES libros (id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_capitulos_libro ON capitulos (libro_id, numero);

-- ---------------------------------------------------------------------------
-- Versículos
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS versiculos (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    capitulo_id     INTEGER NOT NULL,
    numero          INTEGER NOT NULL,             -- 1..N
    texto           TEXT    NOT NULL,
    UNIQUE (capitulo_id, numero),
    FOREIGN KEY (capitulo_id) REFERENCES capitulos (id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_versiculos_cap ON versiculos (capitulo_id, numero);

-- ---------------------------------------------------------------------------
-- Vista de cita por referencia textual ("Salmo 100:4")
-- Permite consulta rápida sin JOIN en runtime caliente.
-- ---------------------------------------------------------------------------
CREATE VIEW IF NOT EXISTS v_citas AS
SELECT
    l.nombre   AS libro,
    l.abrev3   AS abrev3,
    l.abrev2   AS abrev2,
    c.numero   AS capitulo,
    v.numero   AS versiculo,
    v.texto    AS texto
FROM versiculos v
JOIN capitulos c ON c.id = v.capitulo_id
JOIN libros    l ON l.id = c.libro_id;

-- ---------------------------------------------------------------------------
-- Índice FTS5 para búsqueda de texto (búsqueda ≤200 ms — P2)
-- ---------------------------------------------------------------------------
CREATE VIRTUAL TABLE IF NOT EXISTS versiculos_fts USING fts5(
    texto,
    content='versiculos',
    content_rowid='id',
    tokenize='unicode61 remove_diacritics 2'
);

-- Triggers para mantener el FTS sincronizado
CREATE TRIGGER IF NOT EXISTS trg_versiculos_ai AFTER INSERT ON versiculos BEGIN
    INSERT INTO versiculos_fts(rowid, texto) VALUES (new.id, new.texto);
END;

CREATE TRIGGER IF NOT EXISTS trg_versiculos_ad AFTER DELETE ON versiculos BEGIN
    INSERT INTO versiculos_fts(versiculos_fts, rowid, texto) VALUES ('delete', old.id, old.texto);
END;

CREATE TRIGGER IF NOT EXISTS trg_versiculos_au AFTER UPDATE ON versiculos BEGIN
    INSERT INTO versiculos_fts(versiculos_fts, rowid, texto) VALUES ('delete', old.id, old.texto);
    INSERT INTO versiculos_fts(rowid, texto) VALUES (new.id, new.texto);
END;

-- ---------------------------------------------------------------------------
-- Favoritos (lista del operador)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS favoritos (
    cita            TEXT PRIMARY KEY,             -- "Juan 3:16"
    fecha           TEXT NOT NULL DEFAULT (datetime('now'))
);

-- ---------------------------------------------------------------------------
-- Metadatos de la biblia
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS biblia_meta (
    clave       TEXT PRIMARY KEY,
    valor       TEXT NOT NULL
);

-- Ejemplo de claves esperadas:
--   nombre          -> "Reina-Valera 1909"
--   abrev           -> "RVR1909"
--   idioma          -> "es"
--   derechos        -> "Dominio público"
--   fuente          -> "Zefania XML"
--   total_versiculos -> 31102
--   fecha_import    -> datetime

INSERT OR IGNORE INTO biblia_meta (clave, valor) VALUES ('esquema', 'bible');
INSERT OR IGNORE INTO biblia_meta (clave, valor) VALUES ('version', '1');

-- ---------------------------------------------------------------------------
-- Datos semilla: los 66 libros (orden canónico)
-- Esta tabla se llena siempre al crear la BD; el importador solo llena
-- capitulos y versiculos.
-- ---------------------------------------------------------------------------
INSERT OR IGNORE INTO libros (id, nombre, abrev3, abrev2, testamento) VALUES
-- Antiguo Testamento
( 1, 'Génesis',         'Gén', 'Gn', 'AT'),
( 2, 'Éxodo',           'Éxo', 'Ex', 'AT'),
( 3, 'Levítico',        'Lev', 'Lv', 'AT'),
( 4, 'Números',         'Núm', 'Nm', 'AT'),
( 5, 'Deuteronomio',    'Deu', 'Dt', 'AT'),
( 6, 'Josué',           'Jos', 'Jos','AT'),
( 7, 'Jueces',          'Jue', 'Jue','AT'),
( 8, 'Rut',             'Rut', 'Rut','AT'),
( 9, '1 Samuel',        '1Sa', '1Sa','AT'),
(10, '2 Samuel',        '2Sa', '2Sa','AT'),
(11, '1 Reyes',         '1Re', '1Re','AT'),
(12, '2 Reyes',         '2Re', '2Re','AT'),
(13, '1 Crónicas',      '1Cr', '1Cr','AT'),
(14, '2 Crónicas',      '2Cr', '2Cr','AT'),
(15, 'Esdras',          'Esd', 'Esd','AT'),
(16, 'Nehemías',        'Neh', 'Neh','AT'),
(17, 'Ester',           'Est', 'Est','AT'),
(18, 'Job',             'Job', 'Job','AT'),
(19, 'Salmos',          'Sal', 'Sal','AT'),
(20, 'Proverbios',      'Pro', 'Pr', 'AT'),
(21, 'Eclesiastés',     'Ecl', 'Ec', 'AT'),
(22, 'Cantares',        'Cnt', 'Cnt','AT'),
(23, 'Isaías',          'Isa', 'Is', 'AT'),
(24, 'Jeremías',        'Jer', 'Jer','AT'),
(25, 'Lamentaciones',   'Lam', 'Lm', 'AT'),
(26, 'Ezequiel',        'Eze', 'Ez', 'AT'),
(27, 'Daniel',          'Dan', 'Dn', 'AT'),
(28, 'Oseas',            'Ose', 'Os', 'AT'),
(29, 'Joel',            'Joe', 'Jl', 'AT'),
(30, 'Amós',            'Amo', 'Am', 'AT'),
(31, 'Abdías',           'Abd', 'Abd','AT'),
(32, 'Jonás',            'Jon', 'Jon','AT'),
(33, 'Miqueas',         'Miq', 'Mi', 'AT'),
(34, 'Nahúm',           'Nah', 'Nah','AT'),
(35, 'Habacuc',         'Hab', 'Hab','AT'),
(36, 'Sofonías',        'Sof', 'Sof','AT'),
(37, 'Hageo',           'Hag', 'Hag','AT'),
(38, 'Zacarías',        'Zac', 'Zac','AT'),
(39, 'Malaquías',       'Mal', 'Mal','AT'),
-- Nuevo Testamento
(40, 'Mateo',           'Mat', 'Mt', 'NT'),
(41, 'Marcos',          'Mar', 'Mr', 'NT'),
(42, 'Lucas',           'Luc', 'Lc', 'NT'),
(43, 'Juan',            'Jua', 'Jn', 'NT'),
(44, 'Hechos',          'Hch', 'Hch','NT'),
(45, 'Romanos',         'Rom', 'Ro', 'NT'),
(46, '1 Corintios',     '1Co', '1Co','NT'),
(47, '2 Corintios',     '2Co', '2Co','NT'),
(48, 'Gálatas',         'Gál', 'Gál','NT'),
(49, 'Efesios',         'Efe', 'Ef', 'NT'),
(50, 'Filipenses',      'Fil', 'Fil','NT'),
(51, 'Colosenses',      'Col', 'Col','NT'),
(52, '1 Tesalonicenses','1Te', '1Te','NT'),
(53, '2 Tesalonicenses','2Te', '2Te','NT'),
(54, '1 Timoteo',       '1Ti', '1Ti','NT'),
(55, '2 Timoteo',       '2Ti', '2Ti','NT'),
(56, 'Tito',            'Tit', 'Tit','NT'),
(57, 'Filemón',         'Flm', 'Flm','NT'),
(58, 'Hebreos',         'Heb', 'He','NT'),
(59, 'Santiago',        'Snt', 'St','NT'),
(60, '1 Pedro',         '1Pe', '1Pe','NT'),
(61, '2 Pedro',         '2Pe', '2Pe','NT'),
(62, '1 Juan',          '1Jn', '1Jn','NT'),
(63, '2 Juan',          '2Jn', '2Jn','NT'),
(64, '3 Juan',          '3Jn', '3Jn','NT'),
(65, 'Judas',           'Jud', 'Jud','NT'),
(66, 'Apocalipsis',     'Apo', 'Ap','NT');
