-- Migration 018 : avoir PARTIEL (V1.5). Un avoir rembourse desormais des
-- lignes choisies (quantites partielles admises) - plusieurs avoirs par
-- vente tant que tout n'est pas rembourse. La remise globale de la vente
-- est reprise au prorata du montant rembourse.
-- credit_notes perd donc son UNIQUE(sale_id) : reconstruction de la table
-- (SQLite ne sait pas retirer une contrainte en place).

CREATE TABLE credit_notes_new (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    number TEXT NOT NULL UNIQUE,
    sale_id INTEGER NOT NULL REFERENCES sales(id),
    reason TEXT NOT NULL,
    restock INTEGER NOT NULL DEFAULT 1,
    refund_method TEXT NOT NULL DEFAULT 'cash'
        CHECK (refund_method IN ('cash', 'cheque', 'transfer', 'credit')),
    total INTEGER NOT NULL,
    user_id INTEGER REFERENCES users(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

INSERT INTO credit_notes_new (id, uuid, number, sale_id, reason, restock,
                              refund_method, total, user_id, created_at)
SELECT id, uuid, number, sale_id, reason, restock,
       refund_method, total, user_id, created_at
FROM credit_notes;

DROP TABLE credit_notes;
ALTER TABLE credit_notes_new RENAME TO credit_notes;
CREATE INDEX idx_credit_notes_sale ON credit_notes(sale_id);

-- Lignes remboursees par l'avoir (snapshot au moment de l'avoir)
CREATE TABLE credit_note_lines (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    credit_note_id INTEGER NOT NULL REFERENCES credit_notes(id),
    sale_line_id INTEGER NOT NULL REFERENCES sale_lines(id),
    variant_id INTEGER REFERENCES variants(id),
    label_snapshot TEXT NOT NULL,
    qty INTEGER NOT NULL,
    unit_price INTEGER NOT NULL,
    line_total INTEGER NOT NULL
);

CREATE INDEX idx_credit_note_lines_note ON credit_note_lines(credit_note_id);
CREATE INDEX idx_credit_note_lines_sale_line ON credit_note_lines(sale_line_id);

-- Les avoirs existants etaient TOTAUX : leurs lignes = toutes les lignes
-- de la vente, en entier.
INSERT INTO credit_note_lines (credit_note_id, sale_line_id, variant_id,
                               label_snapshot, qty, unit_price, line_total)
SELECT cn.id, sl.id, sl.variant_id, sl.label_snapshot, sl.qty,
       sl.unit_price, sl.line_total
FROM credit_notes cn
JOIN sale_lines sl ON sl.sale_id = cn.sale_id;
