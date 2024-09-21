#include "controllers/catalog_controller.h"

#include "common/money.h"
#include "generators/label_generator.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QLocale>
#include <QUrl>
#include <QUuid>

namespace nursera {

CatalogController::CatalogController(IProductRepository& products,
                                     ICategoryRepository& categories,
                                     QObject* parent)
    : QObject(parent)
    , m_products(products)
    , m_categories(categories)
{
}

void CatalogController::setSearchTerm(const QString& term)
{
    if (m_searchTerm == term)
        return;
    m_searchTerm = term;
    emit searchTermChanged();
    refresh();
}

void CatalogController::setCategoryFilter(int categoryId)
{
    if (m_categoryFilter == categoryId)
        return;
    m_categoryFilter = categoryId;
    emit categoryFilterChanged();
    refresh();
}

void CatalogController::setShowInactive(bool show)
{
    if (m_showInactive == show)
        return;
    m_showInactive = show;
    emit showInactiveChanged();
    refresh();
}

void CatalogController::refresh()
{
    auto result = m_products.search(m_searchTerm, m_categoryFilter,
                                    m_showInactive);
    if (!result) {
        emit errorOccurred(result.error().message);
        return;
    }
    m_model.setRows(std::move(result.value()));
    emit refreshed();
}

QVariantList CatalogController::categoryOptions() const
{
    QVariantList options;
    const auto categories = m_categories.all();
    if (!categories)
        return options;
    for (const Category& category : categories.value()) {
        options.append(QVariantMap{
            {QStringLiteral("id"), category.id},
            {QStringLiteral("label"), category.nameFr},
        });
    }
    return options;
}

bool CatalogController::createProduct(const QVariantMap& data)
{
    const QString nameFr = data.value(QStringLiteral("nameFr")).toString().trimmed();
    if (nameFr.isEmpty()) {
        emit errorOccurred(tr("Le nom (FR) est obligatoire."));
        return false;
    }

    const qint64 priceMillimes =
        Money::parseMillimes(data.value(QStringLiteral("price")).toString());
    if (priceMillimes < 0) {
        emit errorOccurred(tr("Prix invalide — format attendu : 12,500"));
        return false;
    }

    Product product;
    product.nameFr = nameFr;
    product.nameAr = data.value(QStringLiteral("nameAr")).toString().trimmed();
    product.botanicalName =
        data.value(QStringLiteral("botanicalName")).toString().trimmed();
    product.categoryId = data.value(QStringLiteral("categoryId")).toInt();
    product.type = data.value(QStringLiteral("type")).toString()
                       == QLatin1String("goods")
        ? ProductType::Goods
        : ProductType::Plant;

    Variant variant;
    variant.packaging =
        data.value(QStringLiteral("packaging")).toString().trimmed();
    if (variant.packaging.isEmpty())
        variant.packaging = QStringLiteral("godet");
    variant.priceTtc = Money::fromMillimes(priceMillimes);
    variant.vatRatePercent = data.value(QStringLiteral("vatRate")).toInt();

    const auto created = m_products.insertWithVariants(product, {variant});
    if (!created) {
        emit errorOccurred(created.error().message);
        return false;
    }

    refresh();
    emit productCreated(created.value());
    return true;
}

namespace {

QString typeCode(ProductType type)
{
    return type == ProductType::Goods ? QStringLiteral("goods")
                                      : QStringLiteral("plant");
}

// millimes -> "12,500" pour les champs d'édition (sans " DT").
QString priceField(const Money& money)
{
    const qint64 m = money.millimes();
    return QStringLiteral("%1,%2").arg(m / 1000).arg(m % 1000, 3, 10,
                                                     QLatin1Char('0'));
}

} // namespace

QVariantMap CatalogController::productDetail(int productId)
{
    const auto product = m_products.byId(productId);
    if (!product) {
        emit errorOccurred(product.error().message);
        return {};
    }
    const Product& p = product.value();
    return QVariantMap{
        {QStringLiteral("id"), p.id},
        {QStringLiteral("nameFr"), p.nameFr},
        {QStringLiteral("nameAr"), p.nameAr},
        {QStringLiteral("botanicalName"), p.botanicalName},
        {QStringLiteral("categoryId"), p.categoryId},
        {QStringLiteral("type"), typeCode(p.type)},
        {QStringLiteral("descriptionFr"), p.descriptionFr},
        {QStringLiteral("descriptionAr"), p.descriptionAr},
        {QStringLiteral("active"), p.active},
    };
}

QVariantList CatalogController::variantsOf(int productId)
{
    QVariantList list;
    const auto variants = m_products.variantsOf(productId);
    if (!variants) {
        emit errorOccurred(variants.error().message);
        return list;
    }
    for (const Variant& v : variants.value()) {
        list.append(QVariantMap{
            {QStringLiteral("id"), v.id},
            {QStringLiteral("sku"), v.sku},
            {QStringLiteral("barcode"), v.barcode},
            {QStringLiteral("packaging"), v.packaging},
            {QStringLiteral("price"), priceField(v.priceTtc)},
            {QStringLiteral("pricePro"),
             v.priceProTtc.millimes() > 0 ? priceField(v.priceProTtc) : QString()},
            {QStringLiteral("vatRate"), v.vatRatePercent},
            {QStringLiteral("alertThreshold"), v.alertThreshold},
            {QStringLiteral("active"), v.active},
        });
    }
    return list;
}

bool CatalogController::saveProduct(const QVariantMap& data)
{
    const int id = data.value(QStringLiteral("id")).toInt();
    const QString nameFr = data.value(QStringLiteral("nameFr")).toString().trimmed();
    if (id <= 0 || nameFr.isEmpty()) {
        emit errorOccurred(tr("Le nom (FR) est obligatoire."));
        return false;
    }

    const auto existing = m_products.byId(id);
    if (!existing) {
        emit errorOccurred(existing.error().message);
        return false;
    }
    Product product = existing.value();
    product.nameFr = nameFr;
    product.nameAr = data.value(QStringLiteral("nameAr")).toString().trimmed();
    product.botanicalName =
        data.value(QStringLiteral("botanicalName")).toString().trimmed();
    product.categoryId = data.value(QStringLiteral("categoryId")).toInt();
    product.type = data.value(QStringLiteral("type")).toString()
                       == QLatin1String("goods")
        ? ProductType::Goods
        : ProductType::Plant;
    product.descriptionFr =
        data.value(QStringLiteral("descriptionFr")).toString().trimmed();
    product.descriptionAr =
        data.value(QStringLiteral("descriptionAr")).toString().trimmed();

    if (const auto saved = m_products.update(product); !saved) {
        emit errorOccurred(saved.error().message);
        return false;
    }
    refresh();
    emit productSaved();
    return true;
}

bool CatalogController::isDeletable(int productId)
{
    const auto referenced = m_products.isReferenced(productId);
    return referenced.isOk() && !referenced.value();
}

bool CatalogController::deleteProduct(int productId)
{
    // Photo orpheline nettoyée avant la suppression en base.
    const auto photo = m_products.mainPhotoPath(productId);
    const auto removed = m_products.remove(productId);
    if (!removed) {
        emit errorOccurred(removed.error().message);
        return false;
    }
    if (photo.isOk() && !photo.value().isEmpty())
        QFile::remove(photo.value());
    refresh();
    return true;
}

bool CatalogController::setProductActive(int productId, bool active)
{
    if (const auto result = m_products.setActive(productId, active); !result) {
        emit errorOccurred(result.error().message);
        return false;
    }
    refresh();
    emit productSaved();
    return true;
}

QString CatalogController::photoUrl(int productId)
{
    const auto path = m_products.mainPhotoPath(productId);
    if (!path || path.value().isEmpty())
        return {};
    return QUrl::fromLocalFile(path.value()).toString();
}

bool CatalogController::setProductPhoto(int productId, const QString& sourceUrl)
{
    if (productId <= 0 || m_photoDir.isEmpty())
        return false;

    const QString sourcePath = QUrl(sourceUrl).toLocalFile();
    QImage image(sourcePath);
    if (image.isNull()) {
        emit errorOccurred(tr("Image illisible."));
        return false;
    }

    // Redimensionnement ≤ 1600 px ; le ré-encodage JPEG efface l'EXIF.
    if (image.width() > 1600 || image.height() > 1600)
        image = image.scaled(1600, 1600, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);

    if (!QDir().mkpath(m_photoDir)) {
        emit errorOccurred(tr("Dossier photos inaccessible."));
        return false;
    }
    const QString dest = m_photoDir + QLatin1Char('/')
        + QUuid::createUuid().toString(QUuid::WithoutBraces)
        + QStringLiteral(".jpg");
    if (!image.save(dest, "JPEG", 85)) {
        emit errorOccurred(tr("Enregistrement de l'image impossible."));
        return false;
    }

    if (const auto saved = m_products.setMainPhoto(productId, dest); !saved) {
        emit errorOccurred(saved.error().message);
        return false;
    }
    refresh();
    emit productSaved();
    return true;
}

QString CatalogController::printLabels(int productId)
{
    if (m_labelDir.isEmpty())
        return {};
    const auto product = m_products.byId(productId);
    const auto variants = m_products.variantsOf(productId);
    if (!product || !variants) {
        emit errorOccurred(tr("Produit introuvable."));
        return {};
    }

    const QLocale locale;
    QList<LabelGenerator::Label> labels;
    for (const Variant& v : variants.value()) {
        if (!v.active)
            continue;
        LabelGenerator::Label label;
        label.qrData = v.barcode.isEmpty() ? v.sku : v.barcode;
        label.nameFr = QStringLiteral("%1 — %2")
                           .arg(product.value().nameFr, v.packaging);
        label.priceDisplay = v.priceTtc.toDisplayString(locale);
        label.sku = v.sku;
        labels.append(label);
    }
    if (labels.isEmpty()) {
        emit errorOccurred(tr("Ce produit n'a aucun conditionnement actif."));
        return {};
    }

    const auto pdf = LabelGenerator::generatePdf(labels, m_labelDir);
    if (!pdf) {
        emit errorOccurred(pdf.error().message);
        return {};
    }
    return QUrl::fromLocalFile(pdf.value()).toString();
}

bool CatalogController::saveVariant(const QVariantMap& data)
{
    const int productId = data.value(QStringLiteral("productId")).toInt();
    const QString packaging =
        data.value(QStringLiteral("packaging")).toString().trimmed();
    if (productId <= 0 || packaging.isEmpty()) {
        emit errorOccurred(tr("Le conditionnement est obligatoire."));
        return false;
    }

    const qint64 price =
        Money::parseMillimes(data.value(QStringLiteral("price")).toString());
    if (price < 0) {
        emit errorOccurred(tr("Prix invalide — format attendu : 12,500"));
        return false;
    }
    const QString proText = data.value(QStringLiteral("pricePro")).toString().trimmed();
    qint64 pricePro = 0;
    if (!proText.isEmpty()) {
        pricePro = Money::parseMillimes(proText);
        if (pricePro < 0) {
            emit errorOccurred(tr("Prix pro invalide."));
            return false;
        }
    }

    Variant variant;
    variant.id = data.value(QStringLiteral("id")).toInt();
    variant.productId = productId;
    variant.packaging = packaging;
    variant.barcode = data.value(QStringLiteral("barcode")).toString().trimmed();

    // Doublon code-barres = scan de caisse ambigu -> refus.
    if (!variant.barcode.isEmpty()) {
        if (const auto exists = m_products.barcodeExists(variant.barcode,
                                                         variant.id);
            exists.isOk() && exists.value()) {
            emit errorOccurred(
                tr("Ce code-barres est déjà utilisé par un autre article."));
            return false;
        }
    }
    variant.priceTtc = Money::fromMillimes(price);
    variant.priceProTtc = Money::fromMillimes(pricePro);
    variant.vatRatePercent = data.value(QStringLiteral("vatRate")).toInt();
    variant.alertThreshold =
        data.contains(QStringLiteral("alertThreshold"))
            ? data.value(QStringLiteral("alertThreshold")).toInt()
            : -1;
    variant.active = data.value(QStringLiteral("active"), true).toBool();

    const auto result = variant.id > 0 ? m_products.updateVariant(variant)
                                       : [&] {
        const auto inserted = m_products.insertVariant(variant);
        return inserted ? Result<void>::ok()
                        : Result<void>::fail(inserted.error());
    }();
    if (!result) {
        emit errorOccurred(result.error().message);
        return false;
    }
    refresh();
    emit variantSaved();
    return true;
}

} // namespace nursera
