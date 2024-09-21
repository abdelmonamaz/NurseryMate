-- Migration 017 : paiements fournisseurs (F07-05) - miroir de l'encours
-- client. Dette fournisseur = SUM(receptions.total_cost) - SUM(paiements).
-- cheque_due : date d'echeance du cheque (suivi des echeances a venir).

CREATE TABLE supplier_payments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    supplier_id INTEGER NOT NULL REFERENCES suppliers(id),
    amount INTEGER NOT NULL,
    method TEXT NOT NULL DEFAULT 'cash'
        CHECK (method IN ('cash', 'cheque', 'transfer')),
    cheque_number TEXT,
    cheque_due TEXT,
    note TEXT,
    user_id INTEGER REFERENCES users(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_supplier_payments_supplier ON supplier_payments(supplier_id);
CREATE INDEX idx_supplier_payments_due ON supplier_payments(cheque_due);
