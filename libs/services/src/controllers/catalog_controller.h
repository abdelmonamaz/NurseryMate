#pragma once

#include "qml_models/product_list_model.h"
#include "repositories/icategory_repository.h"
#include "repositories/iproduct_repository.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace nursera {

// Controller de l'écran Catalogue (M02) — un controller par écran
// (doc 02 §2.2). Exposé à QML via qmlRegisterSingletonInstance.
class CatalogController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(nursera::ProductListModel* products READ products CONSTANT)
    Q_PROPERTY(QString searchTerm READ searchTerm WRITE setSearchTerm
                   NOTIFY searchTermChanged)
    Q_PROPERTY(int categoryFilter READ categoryFilter WRITE setCategoryFilter
                   NOTIFY categoryFilterChanged)
    Q_PROPERTY(bool showInactive READ showInactive WRITE setShowInactive
                   NOTIFY showInactiveChanged)

public:
    CatalogController(IProductRepository& products,
                      ICategoryRepository& categories,
                      QObject* parent = nullptr);

    // Dossier de stockage des photos (F02-01).
    void setPhotoDir(const QString& dir) { m_photoDir = dir; }
    // Dossier de sortie des planches d'étiquettes (F02-07).
    void setLabelDir(const QString& dir) { m_labelDir = dir; }

    ProductListModel* products() { return &m_model; }

    QString searchTerm() const { return m_searchTerm; }
    void setSearchTerm(const QString& term);

    int categoryFilter() const { return m_categoryFilter; }
    void setCategoryFilter(int categoryId);

    bool showInactive() const { return m_showInactive; }
    void setShowInactive(bool show);

    Q_INVOKABLE void refresh();

    // [{id, label}] pour les ComboBox (label FR pour l'instant,
    // sélection par locale avec le module i18n).
    Q_INVOKABLE QVariantList categoryOptions() const;

    // data : nameFr*, nameAr, botanicalName, categoryId, type ("plant"|"goods"),
    //        packaging, price ("12,500"), vatRate.
    // Émet productCreated ou errorOccurred.
    Q_INVOKABLE bool createProduct(const QVariantMap& data);

    // ── Édition (fiche produit, F02) ──────────────────────────
    // Champs du produit pour l'écran d'édition.
    Q_INVOKABLE QVariantMap productDetail(int productId);
    // Conditionnements du produit : [{id, sku, barcode, packaging, price,
    // pricePro, vatRate, alertThreshold, active}].
    Q_INVOKABLE QVariantList variantsOf(int productId);

    // data avec `id` : met à jour le produit (nom FR/AR, botanique,
    // catégorie, type, descriptions). Émet productSaved.
    Q_INVOKABLE bool saveProduct(const QVariantMap& data);
    Q_INVOKABLE bool setProductActive(int productId, bool active);
    // Ajout fautif jamais référencé : suppression physique admise (norme).
    Q_INVOKABLE bool isDeletable(int productId);
    Q_INVOKABLE bool deleteProduct(int productId);

    // data : productId*, id (0 = nouveau), packaging*, price ("12,500"),
    // pricePro, vatRate, barcode, alertThreshold (-1 = aucun), active.
    // Émet variantSaved.
    Q_INVOKABLE bool saveVariant(const QVariantMap& data);

    // Photo principale (F02-01) : url file:// existante, ou "".
    Q_INVOKABLE QString photoUrl(int productId);
    // Copie l'image choisie (redimensionnée ≤ 1600 px, ré-encodée JPEG
    // — EXIF nettoyé) et l'associe au produit. Émet productSaved.
    Q_INVOKABLE bool setProductPhoto(int productId, const QString& sourceUrl);

    // Planche d'étiquettes QR d'un produit (une par conditionnement,
    // F02-07). Retourne l'url file:// du PDF, ou "".
    Q_INVOKABLE QString printLabels(int productId);

signals:
    void searchTermChanged();
    void categoryFilterChanged();
    void showInactiveChanged();
    void refreshed();
    void productCreated(int productId);
    void productSaved();
    void variantSaved();
    void errorOccurred(const QString& message);

private:
    IProductRepository& m_products;
    ICategoryRepository& m_categories;
    ProductListModel m_model;
    QString m_searchTerm;
    int m_categoryFilter = 0;
    bool m_showInactive = false;
    QString m_photoDir;
    QString m_labelDir;
};

} // namespace nursera
