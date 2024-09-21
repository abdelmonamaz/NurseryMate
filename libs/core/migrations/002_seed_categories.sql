-- Migration 002 : seed des catégories racines (annexe A, doc 01)
-- Modifiables ensuite via l'administration (F11-06)

INSERT INTO categories (name_fr, name_ar, sort_order) VALUES
    ('Arbres fruitiers', 'أشجار مثمرة', 1),
    ('Arbres & arbustes d''ornement', 'أشجار وشجيرات الزينة', 2),
    ('Palmiers & exotiques', 'نخيل ونباتات استوائية', 3),
    ('Plantes méditerranéennes & aromatiques', 'نباتات متوسطية وعطرية', 4),
    ('Plantes d''intérieur', 'نباتات داخلية', 5),
    ('Fleurs saisonnières & vivaces', 'أزهار موسمية ومعمرة', 6),
    ('Cactus & succulentes', 'صبار وعصاريات', 7),
    ('Plants potagers', 'شتلات خضروات', 8),
    ('Gazon & couvre-sols', 'عشب ومغطيات التربة', 9),
    ('Fournitures', 'مستلزمات', 10);
