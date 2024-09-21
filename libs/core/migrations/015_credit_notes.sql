-- Migration 015 : avoirs / notes de credit (norme comptable - une vente ne
-- se supprime JAMAIS, elle se contre-passe par un document inverse numerote).
-- Un seul avoir par vente (avoir total V1). refund_method :
--   cash/cheque/transfer = remboursement au client (sort de la caisse du jour)
--   credit = impute sur l'encours du client (vente a credit corrigee)

CREATE TABLE credit_notes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    number TEXT NOT NULL UNIQUE,
    sale_id INTEGER NOT NULL UNIQUE REFERENCES sales(id),
    reason TEXT NOT NULL,
    restock INTEGER NOT NULL DEFAULT 1,
    refund_method TEXT NOT NULL DEFAULT 'cash'
        CHECK (refund_method IN ('cash', 'cheque', 'transfer', 'credit')),
    total INTEGER NOT NULL,
    user_id INTEGER REFERENCES users(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_credit_notes_sale ON credit_notes(sale_id);
