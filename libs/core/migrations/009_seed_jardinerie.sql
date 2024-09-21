-- Migration 009 : catégories jardinerie / articles non-plantes (M02)
-- La pépinière vend aussi outillage, motoculture, phytosanitaires, etc.

INSERT INTO categories (name_fr, name_ar, sort_order) VALUES
    ('Outillage de jardin', 'أدوات البستنة', 11),
    ('Motoculture & mécanique', 'معدات آلية وميكانيكا', 12),
    ('Produits phytosanitaires', 'منتجات وقاية النباتات', 13),
    ('Engrais & amendements', 'أسمدة ومحسّنات التربة', 14),
    ('Arrosage & irrigation', 'الري والسقي', 15);
