-- Migration 012 : devis (M05 — F05-01/02)
-- Un devis ne touche jamais le stock (RG-05.b). Il peut contenir des
-- lignes produit ou des lignes libres (prestation d'aménagement).

CREATE TABLE quotes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    number TEXT NOT NULL UNIQUE,
    customer_id INTEGER REFERENCES customers(id),
    customer_name TEXT,
    status TEXT NOT NULL DEFAULT 'draft'
        CHECK (status IN ('draft', 'sent', 'accepted', 'refused', 'expired')),
    subtotal INTEGER NOT NULL DEFAULT 0,
    discount INTEGER NOT NULL DEFAULT 0,
    total INTEGER NOT NULL DEFAULT 0,
    valid_until TEXT,
    note TEXT,
    user_id INTEGER REFERENCES users(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    pdf_path TEXT
);

CREATE TABLE quote_lines (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    quote_id INTEGER NOT NULL REFERENCES quotes(id),
    variant_id INTEGER REFERENCES variants(id),  -- NULL = ligne libre
    label TEXT NOT NULL,
    qty INTEGER NOT NULL,
    unit_price INTEGER NOT NULL,
    line_total INTEGER NOT NULL
);

CREATE INDEX idx_quotes_customer ON quotes(customer_id);
CREATE INDEX idx_quote_lines_quote ON quote_lines(quote_id);
