#pragma once

#include "models/product.h"

#include <QAbstractListModel>
#include <QList>

namespace nursera {

// Modèle de la liste catalogue (doc 02 §2.2 : jamais de ListModel QML
// pour des données métier).
class ProductListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ProductIdRole = Qt::UserRole + 1,
        NameFrRole,
        NameArRole,
        BotanicalNameRole,
        CategoryFrRole,
        CategoryArRole,
        IsPlantRole,
        VariantCountRole,
        MinPriceRole, // chaîne formatée via Money (RT-01)
        PhotoUrlRole, // file:// de la photo principale, ou ""
        ActiveRole,
    };

    explicit ProductListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QList<ProductRow> rows);
    const ProductRow* rowAt(int index) const;

private:
    QList<ProductRow> m_rows;
};

} // namespace nursera
