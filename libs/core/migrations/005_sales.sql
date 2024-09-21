-- Migration 005 : ventes comptoir (M04) — sales, sale_lines, payments
-- customer_id sera ajouté avec le module Clients (M06)

CREATE TABLE sales (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    number TEXT NOT NULL UNIQUE,
    status TEXT NOT NULL DEFAULT 'completed' CHECK (status IN ('completed', 'cancelled')),
    subtotal INTEGER NOT NULL,
    discount INTEGER NOT NULL DEFAULT 0,
    vat_total INTEGER NOT NULL DEFAULT 0,
    total INTEGER NOT NULL,
    paid_total INTEGER NOT NULL DEFAULT 0,
    user_id INTEGER REFERENCES users(id),
    device_id INTEGER REFERENCES devices(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    cancelled_at TEXT,
    cancel_reason TEXT
);

CREATE TABLE sale_lines (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sale_id INTEGER NOT NULL REFERENCES sales(id),
    variant_id INTEGER REFERENCES variants(id),
    label_snapshot TEXT NOT NULL,
    qty INTEGER NOT NULL,
    unit_price INTEGER NOT NULL,
    vat_rate INTEGER NOT NULL DEFAULT 0,
    discount_bp INTEGER NOT NULL DEFAULT 0,
    line_total INTEGER NOT NULL
);

CREATE TABLE payments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    sale_id INTEGER REFERENCES sales(id),
    method TEXT NOT NULL CHECK (method IN ('cash', 'cheque', 'transfer')),
    amount INTEGER NOT NULL,
    cheque_number TEXT,
    cheque_bank TEXT,
    cheque_due TEXT,
    user_id INTEGER REFERENCES users(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_sales_created ON sales(created_at);
CREATE INDEX idx_sale_lines_sale ON sale_lines(sale_id);
CREATE INDEX idx_payments_sale ON payments(sale_id);
