-- Migration 010 : clôture de caisse quotidienne (F04-09)
-- Compare le total théorique des espèces du jour au montant compté.

CREATE TABLE cash_closures (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    closed_at TEXT NOT NULL DEFAULT (datetime('now')),
    expected_cash INTEGER NOT NULL,
    counted_cash INTEGER NOT NULL,
    gap INTEGER NOT NULL,
    user_id INTEGER REFERENCES users(id)
);
