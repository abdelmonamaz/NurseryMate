-- Migration 014 : fiche fournisseur enrichie (F07-01)
-- supplies : produits/services que le fournisseur peut fournir (texte libre).
-- latitude/longitude : preparation de la localisation sur carte (feature
-- future - QGIS/OSM). NULL = non renseigne.

ALTER TABLE suppliers ADD COLUMN supplies TEXT;
ALTER TABLE suppliers ADD COLUMN latitude REAL;
ALTER TABLE suppliers ADD COLUMN longitude REAL;
