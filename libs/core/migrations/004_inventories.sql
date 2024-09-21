-- Migration 004 : inventaires guidés par emplacement (F03-06, RG-03.c)

CREATE TABLE inventories (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    location_id INTEGER NOT NULL REFERENCES locations(id),
    status TEXT NOT NULL DEFAULT 'draft' CHECK (status IN ('draft', 'validated', 'cancelled')),
    started_by INTEGER REFERENCES users(id),
    started_at TEXT NOT NULL DEFAULT (datetime('now')),
    validated_by INTEGER REFERENCES users(id),
    validated_at TEXT,
    note TEXT
);

CREATE TABLE inventory_lines (
    inventory_id INTEGER NOT NULL REFERENCES inventories(id),
    variant_id INTEGER NOT NULL REFERENCES variants(id),
    qty_expected INTEGER NOT NULL,
    qty_counted INTEGER,
    PRIMARY KEY (inventory_id, variant_id)
);

CREATE INDEX idx_inventories_location ON inventories(location_id, status);
