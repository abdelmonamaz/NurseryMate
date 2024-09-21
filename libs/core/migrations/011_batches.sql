-- Migration 011 : production & culture (M08 — F08-01/02)
-- Un lot suit une population de plants du semis/bouturage jusqu'au stade
-- vendable. Tant qu'il n'est pas vendable, il vit hors du stock commercial
-- (RG-08.a). Invariant : qty_initial = vendable + pertes + restant (RG-08.b).

CREATE TABLE batches (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    number TEXT NOT NULL UNIQUE,
    product_id INTEGER NOT NULL REFERENCES products(id),
    origin TEXT NOT NULL DEFAULT 'seed'
        CHECK (origin IN ('seed', 'cutting', 'division', 'young_plant')),
    qty_initial INTEGER NOT NULL,
    qty_remaining INTEGER NOT NULL,
    location_id INTEGER REFERENCES locations(id),
    status TEXT NOT NULL DEFAULT 'growing'
        CHECK (status IN ('growing', 'closed')),
    notes TEXT,
    user_id INTEGER REFERENCES users(id),
    started_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE batch_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    batch_id INTEGER NOT NULL REFERENCES batches(id),
    kind TEXT NOT NULL CHECK (kind IN ('loss', 'sellable')),
    qty INTEGER NOT NULL,
    variant_id INTEGER REFERENCES variants(id),   -- destination si vendable
    to_location_id INTEGER REFERENCES locations(id),
    loss_reason TEXT,   -- mortality | disease | frost | breakage | other
    note TEXT,
    user_id INTEGER REFERENCES users(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_batches_status ON batches(status);
CREATE INDEX idx_batch_events_batch ON batch_events(batch_id);
