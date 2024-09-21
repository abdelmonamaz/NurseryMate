-- Migration 008 : factures (M05 — F05-03/04)
-- Une facture est émise à partir d'une vente. Ses lignes viennent de
-- sale_lines (déjà snapshot). L'entête fige le client et les totaux.

CREATE TABLE invoices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    number TEXT NOT NULL UNIQUE,
    kind TEXT NOT NULL DEFAULT 'invoice' CHECK (kind IN ('invoice', 'credit_note')),
    sale_id INTEGER REFERENCES sales(id),
    customer_id INTEGER REFERENCES customers(id),
    customer_name TEXT,
    customer_tax_id TEXT,
    subtotal_ht INTEGER NOT NULL,
    vat_total INTEGER NOT NULL,
    stamp_duty INTEGER NOT NULL DEFAULT 0,
    total INTEGER NOT NULL,
    user_id INTEGER REFERENCES users(id),
    issued_at TEXT NOT NULL DEFAULT (datetime('now')),
    pdf_path TEXT
);

CREATE INDEX idx_invoices_sale ON invoices(sale_id);
CREATE INDEX idx_invoices_customer ON invoices(customer_id);
