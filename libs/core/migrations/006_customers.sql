-- Migration 006 : clients & créances (M06)
-- L'encours d'un client = SUM(sales.total) - SUM(payments.amount) sur son id.
-- Les paiements liés à un client portent toujours customer_id (vente ou
-- règlement d'encours avec sale_id NULL).

CREATE TABLE customers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    kind TEXT NOT NULL DEFAULT 'individual' CHECK (kind IN ('individual', 'professional')),
    name TEXT NOT NULL,
    phone TEXT,
    phone2 TEXT,
    email TEXT,
    address TEXT,
    tax_id TEXT,
    lang TEXT NOT NULL DEFAULT 'fr' CHECK (lang IN ('fr', 'ar')),
    credit_limit INTEGER,
    notes TEXT,
    active INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

ALTER TABLE sales ADD COLUMN customer_id INTEGER REFERENCES customers(id);

ALTER TABLE payments ADD COLUMN customer_id INTEGER REFERENCES customers(id);

CREATE INDEX idx_sales_customer ON sales(customer_id);
CREATE INDEX idx_payments_customer ON payments(customer_id);
CREATE INDEX idx_customers_phone ON customers(phone);
