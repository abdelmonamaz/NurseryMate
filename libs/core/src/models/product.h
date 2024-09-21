#pragma once

#include "common/money.h"

#include <QString>

namespace nursera {

enum class ProductType { Plant, Goods };

// Fiche produit bilingue (F02-01). Les attributs horticoles (F02-05)
// seront ajoutés avec l'écran fiche détaillée.
struct Product
{
    int id = 0;
    int categoryId = 0; // 0 = sans catégorie
    QString nameFr;
    QString nameAr;
    QString botanicalName;
    ProductType type = ProductType::Plant;
    QString descriptionFr;
    QString descriptionAr;
    bool active = true;
};

// Variante de conditionnement (F02-03) : le grain de vente et de stock.
struct Variant
{
    int id = 0;
    int productId = 0;
    QString sku;               // généré PNNNN-CONDITIONNEMENT si vide (RG-02.b)
    QString barcode;
    QString packaging;         // godet, pot10, pot14, motte, racines nues…
    Money priceTtc;
    Money priceProTtc;         // 0 = pas de tarif professionnel
    int vatRatePercent = 0;    // 0 / 7 / 13 / 19
    Money avgCost;
    int alertThreshold = -1;   // -1 = pas de seuil d'alerte
    bool active = true;
};

// Ligne de la liste catalogue : produit + agrégats de variantes.
struct ProductRow
{
    int id = 0;
    QString nameFr;
    QString nameAr;
    QString botanicalName;
    QString categoryFr;
    QString categoryAr;
    ProductType type = ProductType::Plant;
    int variantCount = 0;
    Money minPriceTtc;
    QString photoPath; // photo principale (F02-01), vide si aucune
    bool active = true;
};

} // namespace nursera
