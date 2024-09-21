-- Migration 013 : commandes d'achat formelles (M07 — F07-02, F07-03)
-- Cycle brouillon -> envoyee -> partielle/recue -> cloturee. La reception
-- reutilise la table receipts (lien po_id) pour les entrees de stock + CMP.

CREATE TABLE purchase_orders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    number TEXT NOT NULL UNIQUE,
    supplier_id INTEGER NOT NULL REFERENCES suppliers(id),
    status TEXT NOT NULL DEFAULT 'draft'
        CHECK (status IN ('draft', 'sent', 'partial', 'received', 'cancelled')),
    total_cost INTEGER NOT NULL DEFAULT 0,
    note TEXT,
    user_id INTEGER REFERENCES users(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE purchase_order_lines (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    po_id INTEGER NOT NULL REFERENCES purchase_orders(id),
    variant_id INTEGER NOT NULL REFERENCES variants(id),
    label TEXT NOT NULL,
    qty_ordered INTEGER NOT NULL,
    qty_received INTEGER NOT NULL DEFAULT 0,
    unit_cost INTEGER NOT NULL
);

-- Rattachement d'une reception a la commande honoree (NULL = reception directe)
ALTER TABLE receipts ADD COLUMN po_id INTEGER REFERENCES purchase_orders(id);

CREATE INDEX idx_po_supplier ON purchase_orders(supplier_id);
CREATE INDEX idx_po_lines_po ON purchase_order_lines(po_id);
