#include "repositories/sqlite/sqlite_product_repository.h"

#include <QSqlError>
#include <QSqlQuery>

#include <utility>

namespace nursera {
namespace {

QString typeToString(ProductType type)
{
    return type == ProductType::Goods ? QStringLiteral("goods")
                                      : QStringLiteral("plant");
}

ProductType typeFromString(const QString& text)
{
    return text == QLatin1String("goods") ? ProductType::Goods
                                          : ProductType::Plant;
}

// SKU par défaut : PNNNN-CONDITIONNEMENT (RG-02.b)
QString defaultSku(int productId, const QString& packaging)
{
    QString normalized = packaging.toUpper();
    normalized.remove(QLatin1Char(' '));
    return QStringLiteral("P%1-%2")
        .arg(productId, 4, 10, QLatin1Char('0'))
        .arg(normalized.left(12));
}

Variant variantFromQuery(const QSqlQuery& query)
{
    Variant variant;
    variant.id = query.value(0).toInt();
    variant.productId = query.value(1).toInt();
    variant.sku = query.value(2).toString();
    variant.barcode = query.value(3).toString();
    variant.packaging = query.value(4).toString();
    variant.priceTtc = Money::fromMillimes(query.value(5).toLongLong());
    variant.priceProTtc = Money::fromMillimes(query.value(6).toLongLong());
    variant.vatRatePercent = query.value(7).toInt();
    variant.avgCost = Money::fromMillimes(query.value(8).toLongLong());
    variant.alertThreshold = query.value(9).isNull() ? -1 : query.value(9).toInt();
    variant.active = query.value(10).toBool();
    return variant;
}

void bindProduct(QSqlQuery& query, const Product& product)
{
    query.bindValue(QStringLiteral(":category"),
                    product.categoryId > 0 ? QVariant(product.categoryId) : QVariant());
    query.bindValue(QStringLiteral(":name_fr"), product.nameFr);
    query.bindValue(QStringLiteral(":name_ar"), product.nameAr);
    query.bindValue(QStringLiteral(":botanical"), product.botanicalName);
    query.bindValue(QStringLiteral(":type"), typeToString(product.type));
    query.bindValue(QStringLiteral(":desc_fr"), product.descriptionFr);
    query.bindValue(QStringLiteral(":desc_ar"), product.descriptionAr);
    query.bindValue(QStringLiteral(":active"), product.active ? 1 : 0);
}

void bindVariant(QSqlQuery& query, const Variant& variant)
{
    query.bindValue(QStringLiteral(":product"), variant.productId);
    query.bindValue(QStringLiteral(":sku"), variant.sku);
    query.bindValue(QStringLiteral(":barcode"),
                    variant.barcode.isEmpty() ? QVariant() : QVariant(variant.barcode));
    query.bindValue(QStringLiteral(":packaging"), variant.packaging);
    query.bindValue(QStringLiteral(":price"), variant.priceTtc.millimes());
    query.bindValue(QStringLiteral(":price_pro"),
                    variant.priceProTtc.millimes() > 0
                        ? QVariant(variant.priceProTtc.millimes())
                        : QVariant());
    query.bindValue(QStringLiteral(":vat"), variant.vatRatePercent);
    query.bindValue(QStringLiteral(":avg_cost"), variant.avgCost.millimes());
    query.bindValue(QStringLiteral(":alert"),
                    variant.alertThreshold >= 0 ? QVariant(variant.alertThreshold)
                                                : QVariant());
    query.bindValue(QStringLiteral(":active"), variant.active ? 1 : 0);
}

} // namespace

SqliteProductRepository::SqliteProductRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

Result<QList<ProductRow>> SqliteProductRepository::search(const QString& term,
                                                          int categoryId,
                                                          bool includeInactive)
{
    QString sql = QStringLiteral(
        "SELECT p.id, p.name_fr, p.name_ar, p.botanical_name, p.type, p.active, "
        "       c.name_fr, c.name_ar, "
        "       COUNT(v.id), MIN(v.price_ttc), "
        "       COALESCE((SELECT file_path FROM product_photos pp "
        "                 WHERE pp.product_id = p.id AND pp.is_main = 1 LIMIT 1), '') "
        "FROM products p "
        "LEFT JOIN categories c ON c.id = p.category_id "
        "LEFT JOIN variants v ON v.product_id = p.id AND v.active = 1 ");

    QStringList where;
    const QString trimmedTerm = term.trimmed();
    if (!includeInactive)
        where << QStringLiteral("p.active = 1");
    if (categoryId > 0)
        where << QStringLiteral("p.category_id = :category");
    if (!trimmedTerm.isEmpty())
        where << QStringLiteral(
            "(p.name_fr LIKE :t1 OR p.name_ar LIKE :t2 OR p.botanical_name LIKE :t3 "
            "OR EXISTS (SELECT 1 FROM variants vs "
            "           WHERE vs.product_id = p.id "
            "           AND (vs.sku LIKE :t4 OR vs.barcode LIKE :t5)))");
    if (!where.isEmpty())
        sql += QStringLiteral("WHERE ") + where.join(QStringLiteral(" AND ")) + QLatin1Char(' ');
    sql += QStringLiteral("GROUP BY p.id ORDER BY p.name_fr COLLATE NOCASE");

    QSqlQuery query(db());
    if (!query.prepare(sql))
        return Result<QList<ProductRow>>::fail(QStringLiteral("product.search"),
                                               query.lastError().text());
    if (categoryId > 0)
        query.bindValue(QStringLiteral(":category"), categoryId);
    if (!trimmedTerm.isEmpty()) {
        const QString like = QLatin1Char('%') + trimmedTerm + QLatin1Char('%');
        query.bindValue(QStringLiteral(":t1"), like);
        query.bindValue(QStringLiteral(":t2"), like);
        query.bindValue(QStringLiteral(":t3"), like);
        query.bindValue(QStringLiteral(":t4"), like);
        query.bindValue(QStringLiteral(":t5"), like);
    }
    if (!query.exec())
        return Result<QList<ProductRow>>::fail(QStringLiteral("product.search"),
                                               query.lastError().text());

    QList<ProductRow> rows;
    while (query.next()) {
        ProductRow row;
        row.id = query.value(0).toInt();
        row.nameFr = query.value(1).toString();
        row.nameAr = query.value(2).toString();
        row.botanicalName = query.value(3).toString();
        row.type = typeFromString(query.value(4).toString());
        row.active = query.value(5).toBool();
        row.categoryFr = query.value(6).toString();
        row.categoryAr = query.value(7).toString();
        row.variantCount = query.value(8).toInt();
        row.minPriceTtc = Money::fromMillimes(query.value(9).toLongLong());
        row.photoPath = query.value(10).toString();
        rows.append(row);
    }
    return Result<QList<ProductRow>>::ok(std::move(rows));
}

Result<Product> SqliteProductRepository::byId(int id)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, category_id, name_fr, name_ar, botanical_name, type, "
        "       description_fr, description_ar, active "
        "FROM products WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return Result<Product>::fail(QStringLiteral("product.byId"),
                                     query.lastError().text());
    if (!query.next())
        return Result<Product>::fail(QStringLiteral("product.notFound"),
                                     QStringLiteral("Produit %1 introuvable").arg(id));

    Product product;
    product.id = query.value(0).toInt();
    product.categoryId = query.value(1).toInt();
    product.nameFr = query.value(2).toString();
    product.nameAr = query.value(3).toString();
    product.botanicalName = query.value(4).toString();
    product.type = typeFromString(query.value(5).toString());
    product.descriptionFr = query.value(6).toString();
    product.descriptionAr = query.value(7).toString();
    product.active = query.value(8).toBool();
    return Result<Product>::ok(std::move(product));
}

Result<int> SqliteProductRepository::insert(const Product& product)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO products (category_id, name_fr, name_ar, botanical_name, type, "
        "                      description_fr, description_ar, active) "
        "VALUES (:category, :name_fr, :name_ar, :botanical, :type, "
        "        :desc_fr, :desc_ar, :active)"));
    bindProduct(query, product);
    if (!query.exec())
        return Result<int>::fail(QStringLiteral("product.insert"),
                                 query.lastError().text());
    return Result<int>::ok(query.lastInsertId().toInt());
}

Result<void> SqliteProductRepository::update(const Product& product)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE products SET category_id = :category, name_fr = :name_fr, "
        "name_ar = :name_ar, botanical_name = :botanical, type = :type, "
        "description_fr = :desc_fr, description_ar = :desc_ar, active = :active "
        "WHERE id = :id"));
    bindProduct(query, product);
    query.bindValue(QStringLiteral(":id"), product.id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("product.update"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<void> SqliteProductRepository::setActive(int id, bool active)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE products SET active = :active WHERE id = :id"));
    query.bindValue(QStringLiteral(":active"), active ? 1 : 0);
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("product.setActive"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<int> SqliteProductRepository::insertWithVariants(const Product& product,
                                                        QList<Variant> variants)
{
    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<int>::fail(QStringLiteral("product.tx"),
                                 database.lastError().text());

    const auto inserted = insert(product);
    if (!inserted) {
        database.rollback();
        return inserted;
    }
    const int productId = inserted.value();

    for (Variant& variant : variants) {
        variant.productId = productId;
        if (variant.sku.isEmpty())
            variant.sku = defaultSku(productId, variant.packaging);
        if (const auto result = insertVariant(variant); !result) {
            database.rollback();
            return Result<int>::fail(result.error());
        }
    }

    if (!database.commit()) {
        database.rollback();
        return Result<int>::fail(QStringLiteral("product.commit"),
                                 database.lastError().text());
    }
    return Result<int>::ok(productId);
}

Result<QList<Variant>> SqliteProductRepository::variantsOf(int productId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, product_id, sku, barcode, packaging, price_ttc, price_pro_ttc, "
        "       vat_rate, avg_cost, alert_threshold, active "
        "FROM variants WHERE product_id = :product ORDER BY packaging"));
    query.bindValue(QStringLiteral(":product"), productId);
    if (!query.exec())
        return Result<QList<Variant>>::fail(QStringLiteral("variant.of"),
                                            query.lastError().text());

    QList<Variant> variants;
    while (query.next())
        variants.append(variantFromQuery(query));
    return Result<QList<Variant>>::ok(std::move(variants));
}

Result<int> SqliteProductRepository::insertVariant(const Variant& variant)
{
    Variant toInsert = variant;
    if (toInsert.sku.isEmpty())
        toInsert.sku = defaultSku(toInsert.productId, toInsert.packaging);

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO variants (product_id, sku, barcode, packaging, price_ttc, "
        "                      price_pro_ttc, vat_rate, avg_cost, alert_threshold, active) "
        "VALUES (:product, :sku, :barcode, :packaging, :price, "
        "        :price_pro, :vat, :avg_cost, :alert, :active)"));
    bindVariant(query, toInsert);
    if (!query.exec())
        return Result<int>::fail(QStringLiteral("variant.insert"),
                                 query.lastError().text());
    return Result<int>::ok(query.lastInsertId().toInt());
}

Result<QString> SqliteProductRepository::mainPhotoPath(int productId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT file_path FROM product_photos "
        "WHERE product_id = :id AND is_main = 1 LIMIT 1"));
    query.bindValue(QStringLiteral(":id"), productId);
    if (!query.exec())
        return Result<QString>::fail(QStringLiteral("photo.get"),
                                     query.lastError().text());
    return Result<QString>::ok(query.next() ? query.value(0).toString() : QString());
}

Result<void> SqliteProductRepository::setMainPhoto(int productId,
                                                   const QString& filePath)
{
    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<void>::fail(QStringLiteral("photo.tx"),
                                  database.lastError().text());

    // Un seul cliché principal en V1 : on remplace l'existant.
    QSqlQuery clear(database);
    clear.prepare(QStringLiteral(
        "DELETE FROM product_photos WHERE product_id = :id AND is_main = 1"));
    clear.bindValue(QStringLiteral(":id"), productId);
    if (!clear.exec()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("photo.clear"),
                                  clear.lastError().text());
    }

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO product_photos (product_id, file_path, is_main, taken_at) "
        "VALUES (:id, :path, 1, datetime('now'))"));
    insert.bindValue(QStringLiteral(":id"), productId);
    insert.bindValue(QStringLiteral(":path"), filePath);
    if (!insert.exec()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("photo.insert"),
                                  insert.lastError().text());
    }

    if (!database.commit()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("photo.commit"),
                                  database.lastError().text());
    }
    return Result<void>::ok();
}

Result<void> SqliteProductRepository::updateVariant(const Variant& variant)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE variants SET product_id = :product, sku = :sku, barcode = :barcode, "
        "packaging = :packaging, price_ttc = :price, price_pro_ttc = :price_pro, "
        "vat_rate = :vat, avg_cost = :avg_cost, alert_threshold = :alert, "
        "active = :active WHERE id = :id"));
    bindVariant(query, variant);
    query.bindValue(QStringLiteral(":id"), variant.id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("variant.update"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<bool> SqliteProductRepository::isReferenced(int productId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT EXISTS(SELECT 1 FROM sale_lines sl "
        "  JOIN variants v ON v.id = sl.variant_id WHERE v.product_id = :p1) "
        "OR EXISTS(SELECT 1 FROM stock_moves m "
        "  JOIN variants v ON v.id = m.variant_id WHERE v.product_id = :p2) "
        "OR EXISTS(SELECT 1 FROM receipt_lines rl "
        "  JOIN variants v ON v.id = rl.variant_id WHERE v.product_id = :p3) "
        "OR EXISTS(SELECT 1 FROM purchase_order_lines pol "
        "  JOIN variants v ON v.id = pol.variant_id WHERE v.product_id = :p4) "
        "OR EXISTS(SELECT 1 FROM quote_lines ql "
        "  JOIN variants v ON v.id = ql.variant_id WHERE v.product_id = :p5) "
        "OR EXISTS(SELECT 1 FROM batches b WHERE b.product_id = :p6)"));
    for (int i = 1; i <= 6; ++i)
        query.bindValue(QStringLiteral(":p%1").arg(i), productId);
    if (!query.exec() || !query.next())
        return Result<bool>::fail(QStringLiteral("product.referenced"),
                                  query.lastError().text());
    return Result<bool>::ok(query.value(0).toBool());
}

Result<void> SqliteProductRepository::remove(int productId)
{
    // Garde-fou : jamais de suppression d'un produit référencé (norme).
    if (const auto referenced = isReferenced(productId); !referenced)
        return Result<void>::fail(referenced.error());
    else if (referenced.value())
        return Result<void>::fail(
            QStringLiteral("product.referenced"),
            QStringLiteral("Ce produit a un historique — désactivez-le."));

    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<void>::fail(QStringLiteral("product.tx"),
                                  database.lastError().text());
    const auto fail = [&database](const QString& code, const QString& message) {
        database.rollback();
        return Result<void>::fail(code, message);
    };

    // Lignes de stock à zéro éventuelles, photos, variantes, produit.
    for (const QString& sql : {
             QStringLiteral("DELETE FROM stock WHERE variant_id IN "
                            "(SELECT id FROM variants WHERE product_id = :id)"),
             QStringLiteral("DELETE FROM product_photos WHERE product_id = :id"),
             QStringLiteral("DELETE FROM variants WHERE product_id = :id"),
             QStringLiteral("DELETE FROM products WHERE id = :id"),
         }) {
        QSqlQuery query(database);
        query.prepare(sql);
        query.bindValue(QStringLiteral(":id"), productId);
        if (!query.exec())
            return fail(QStringLiteral("product.remove"),
                        query.lastError().text());
    }

    if (!database.commit()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("product.commit"),
                                  database.lastError().text());
    }
    return Result<void>::ok();
}

Result<bool> SqliteProductRepository::barcodeExists(const QString& barcode,
                                                    int excludeVariantId)
{
    if (barcode.trimmed().isEmpty())
        return Result<bool>::ok(false);
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT count(*) FROM variants "
        "WHERE barcode = :barcode AND id != :exclude"));
    query.bindValue(QStringLiteral(":barcode"), barcode.trimmed());
    query.bindValue(QStringLiteral(":exclude"), excludeVariantId);
    if (!query.exec() || !query.next())
        return Result<bool>::fail(QStringLiteral("variant.barcode"),
                                  query.lastError().text());
    return Result<bool>::ok(query.value(0).toInt() > 0);
}

} // namespace nursera
