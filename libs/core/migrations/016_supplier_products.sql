-- Migration 016 : catalogue fournisseur (F07-06) - liens declaratifs
-- fournisseur <-> variantes reelles du catalogue. Les statistiques
-- d'approvisionnement (quantites, prix) restent CALCULEES depuis les
-- receptions (receipt_lines) - aucune saisie en double.

CREATE TABLE supplier_products (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    supplier_id INTEGER NOT NULL REFERENCES suppliers(id),
    variant_id INTEGER NOT NULL REFERENCES variants(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE (supplier_id, variant_id)
);

CREATE INDEX idx_supplier_products_supplier ON supplier_products(supplier_id);
CREATE INDEX idx_supplier_products_variant ON supplier_products(variant_id);
