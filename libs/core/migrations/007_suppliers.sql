-- Migration 007 : fournisseurs & réceptions directes (M07 — F07-01, F07-04)
-- Les commandes d'achat formelles (purchase_orders) viendront en V1.1.

CREATE TABLE suppliers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    phone TEXT,
    email TEXT,
    address TEXT,
    tax_id TEXT,
    payment_terms TEXT,
    notes TEXT,
    active INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE receipts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    supplier_id INTEGER REFERENCES suppliers(id),
    location_id INTEGER NOT NULL REFERENCES locations(id),
    total_cost INTEGER NOT NULL DEFAULT 0,
    note TEXT,
    user_id INTEGER REFERENCES users(id),
    received_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE receipt_lines (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    receipt_id INTEGER NOT NULL REFERENCES receipts(id),
    variant_id INTEGER NOT NULL REFERENCES variants(id),
    qty INTEGER NOT NULL,
    unit_cost INTEGER NOT NULL
);

CREATE INDEX idx_receipts_supplier ON receipts(supplier_id);
CREATE INDEX idx_receipt_lines_receipt ON receipt_lines(receipt_id);
