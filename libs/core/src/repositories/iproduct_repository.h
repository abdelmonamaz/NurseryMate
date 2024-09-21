#pragma once

#include "common/result.h"
#include "models/product.h"

#include <QList>
#include <QString>

namespace nursera {

class IProductRepository
{
public:
    virtual ~IProductRepository() = default;

    // Recherche multilingue FR / AR / nom latin / SKU (F02-06).
    // categoryId <= 0 : toutes les catégories.
    virtual Result<QList<ProductRow>> search(const QString& term,
                                             int categoryId = 0,
                                             bool includeInactive = false) = 0;
    virtual Result<Product> byId(int id) = 0;
    virtual Result<int> insert(const Product& product) = 0;
    virtual Result<void> update(const Product& product) = 0;
    // Désactivation, jamais de suppression (RG-02.a).
    virtual Result<void> setActive(int id, bool active) = 0;

    // Le produit (via ses variantes) apparaît-il dans une transaction
    // (vente, mouvement de stock, réception, commande, devis, lot) ?
    virtual Result<bool> isReferenced(int productId) = 0;
    // Suppression physique — admise UNIQUEMENT pour un ajout fautif jamais
    // référencé (norme). Supprime variantes + photos + produit.
    virtual Result<void> remove(int productId) = 0;

    // Création atomique produit + variantes (transaction). Les SKU vides
    // sont générés à partir de l'id produit (RG-02.b).
    virtual Result<int> insertWithVariants(const Product& product,
                                           QList<Variant> variants) = 0;

    virtual Result<QList<Variant>> variantsOf(int productId) = 0;
    virtual Result<int> insertVariant(const Variant& variant) = 0;
    virtual Result<void> updateVariant(const Variant& variant) = 0;

    // Un code-barres déjà porté par une AUTRE variante ? (contrôle de
    // doublon à la saisie — un doublon rend le scan de caisse ambigu).
    virtual Result<bool> barcodeExists(const QString& barcode,
                                       int excludeVariantId = 0) = 0;

    // Photo principale (F02-01) — un seul cliché principal par produit en V1.
    virtual Result<QString> mainPhotoPath(int productId) = 0;
    virtual Result<void> setMainPhoto(int productId, const QString& filePath) = 0;
};

} // namespace nursera
