-- Migration 019 : traitements & interventions sur les lots (F08-03, V1.5).
-- Journal par lot : arrosage exceptionnel, fertilisation, traitement
-- phytosanitaire (produit + dose), taille, autre.

CREATE TABLE batch_treatments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    batch_id INTEGER NOT NULL REFERENCES batches(id),
    kind TEXT NOT NULL
        CHECK (kind IN ('watering', 'fertilization', 'phyto', 'pruning', 'other')),
    product_used TEXT,
    dose TEXT,
    note TEXT,
    user_id INTEGER REFERENCES users(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_batch_treatments_batch ON batch_treatments(batch_id);
