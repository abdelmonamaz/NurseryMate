#pragma once

#include "repositories/iproduct_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteProductRepository : public IProductRepository
{
public:
    explicit SqliteProductRepository(QString connectionName);

    Result<QList<ProductRow>> search(const QString& term,
                                     int categoryId = 0,
                                     bool includeInactive = false) override;
    Result<Product> byId(int id) override;
    Result<int> insert(const Product& product) override;
    Result<void> update(const Product& product) override;
    Result<void> setActive(int id, bool active) override;

    Result<int> insertWithVariants(const Product& product,
                                   QList<Variant> variants) override;

    Result<QList<Variant>> variantsOf(int productId) override;
    Result<int> insertVariant(const Variant& variant) override;
    Result<void> updateVariant(const Variant& variant) override;
    Result<bool> barcodeExists(const QString& barcode,
                               int excludeVariantId = 0) override;
    Result<bool> isReferenced(int productId) override;
    Result<void> remove(int productId) override;
    Result<QString> mainPhotoPath(int productId) override;
    Result<void> setMainPhoto(int productId, const QString& filePath) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }

    QString m_connectionName;
};

} // namespace nursera
