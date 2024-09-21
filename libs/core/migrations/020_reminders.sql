-- Migration 020 : rappels d'entretien planifiables (F08-04, V1.5).
-- Ex. « Refaire : fertilisation - L-2026-001 dans 15 j », affiches au
-- tableau de bord (echus en premier), marques faits d'un clic.

CREATE TABLE reminders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT NOT NULL UNIQUE,
    label TEXT NOT NULL,
    due_date TEXT NOT NULL,
    batch_id INTEGER REFERENCES batches(id),
    done INTEGER NOT NULL DEFAULT 0,
    user_id INTEGER REFERENCES users(id),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_reminders_due ON reminders(done, due_date);
