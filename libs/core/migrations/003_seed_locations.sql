-- Migration 003 : emplacements par défaut (F03-01)
-- Renommables / extensibles via l'administration (F11-06)

INSERT INTO locations (name_fr, name_ar, kind) VALUES
    ('Serre 1', 'البيت المحمي 1', 'greenhouse'),
    ('Zone de vente', 'منطقة البيع', 'sales_area'),
    ('Dépôt', 'المستودع', 'warehouse');
