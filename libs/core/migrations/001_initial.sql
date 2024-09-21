-- Migration 001 : socle initial — utilisateurs, catalogue, stock, paramètres
-- Référence : docs/02-specifications-techniques.md §3.1
-- Règles d'écriture : une instruction par point-virgule, commentaires
-- uniquement en lignes "--" sans point-virgule (parseur de migrations)

CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    role TEXT NOT NULL CHECK (role IN ('manager', 'seller', 'worker')),
    pin_hash TEXT,
    pwd_hash TEXT,
    lang TEXT NOT NULL DEFAULT 'fr' CHECK (lang IN ('fr', 'ar')),
    active INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    kind TEXT NOT NULL CHECK (kind IN ('desktop', 'mobile')),
    token_hash TEXT NOT NULL,
    last_sync_at TEXT,
    revoked INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE categories (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    parent_id INTEGER REFERENCES categories(id),
    name_fr TEXT NOT NULL,
    name_ar TEXT,
    sort_order INTEGER NOT NULL DEFAULT 0,
    active INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE products (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    category_id INTEGER REFERENCES categories(id),
    name_fr TEXT NOT NULL,
    name_ar TEXT,
    botanical_name TEXT,
    type TEXT NOT NULL DEFAULT 'plant' CHECK (type IN ('plant', 'goods')),
    description_fr TEXT,
    description_ar TEXT,
    sun TEXT,
    water_need INTEGER,
    hardiness TEXT,
    flowering_period TEXT,
    planting_period TEXT,
    adult_height TEXT,
    active INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE product_photos (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    product_id INTEGER NOT NULL REFERENCES products(id),
    file_path TEXT NOT NULL,
    is_main INTEGER NOT NULL DEFAULT 0,
    taken_at TEXT,
    taken_by INTEGER REFERENCES users(id)
);

CREATE TABLE variants (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    product_id INTEGER NOT NULL REFERENCES products(id),
    sku TEXT NOT NULL UNIQUE,
    barcode TEXT,
    packaging TEXT NOT NULL,
    price_ttc INTEGER NOT NULL DEFAULT 0,
    price_pro_ttc INTEGER,
    vat_rate INTEGER NOT NULL DEFAULT 0,
    avg_cost INTEGER NOT NULL DEFAULT 0,
    alert_threshold INTEGER,
    active INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE locations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name_fr TEXT NOT NULL,
    name_ar TEXT,
    kind TEXT NOT NULL CHECK (kind IN ('greenhouse', 'field', 'sales_area', 'warehouse')),
    active INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE stock (
    variant_id INTEGER NOT NULL REFERENCES variants(id),
    location_id INTEGER NOT NULL REFERENCES locations(id),
    qty INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (variant_id, location_id)
);

CREATE TABLE stock_moves (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    variant_id INTEGER NOT NULL REFERENCES variants(id),
    kind TEXT NOT NULL CHECK (kind IN ('in', 'out', 'transfer', 'adjust')),
    from_location_id INTEGER REFERENCES locations(id),
    to_location_id INTEGER REFERENCES locations(id),
    qty INTEGER NOT NULL,
    reason TEXT,
    loss_reason TEXT,
    ref_kind TEXT,
    ref_id INTEGER,
    note TEXT,
    user_id INTEGER REFERENCES users(id),
    device_id INTEGER REFERENCES devices(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    synced_at TEXT
);

CREATE INDEX idx_stock_moves_variant ON stock_moves(variant_id, created_at);
CREATE INDEX idx_stock_moves_created ON stock_moves(created_at);
CREATE INDEX idx_products_category ON products(category_id);
CREATE INDEX idx_variants_product ON variants(product_id);

CREATE TABLE settings (
    key TEXT PRIMARY KEY,
    value TEXT
);

CREATE TABLE doc_counters (
    kind TEXT NOT NULL,
    year INTEGER NOT NULL,
    next_number INTEGER NOT NULL DEFAULT 1,
    PRIMARY KEY (kind, year)
);

CREATE TABLE audit_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER REFERENCES users(id),
    device_id INTEGER REFERENCES devices(id),
    entity TEXT NOT NULL,
    entity_id INTEGER,
    action TEXT NOT NULL,
    details_json TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);
