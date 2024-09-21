#include "qml_models/product_list_model.h"

#include <QLocale>
#include <QUrl>

#include <utility>

namespace nursera {

ProductListModel::ProductListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ProductListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant ProductListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const ProductRow& row = m_rows.at(index.row());
    switch (role) {
    case ProductIdRole: return row.id;
    case NameFrRole: return row.nameFr;
    case NameArRole: return row.nameAr;
    case BotanicalNameRole: return row.botanicalName;
    case CategoryFrRole: return row.categoryFr;
    case CategoryArRole: return row.categoryAr;
    case IsPlantRole: return row.type == ProductType::Plant;
    case VariantCountRole: return row.variantCount;
    case MinPriceRole:
        return row.variantCount > 0 ? row.minPriceTtc.toDisplayString(QLocale())
                                    : QString();
    case PhotoUrlRole:
        return row.photoPath.isEmpty()
            ? QString()
            : QUrl::fromLocalFile(row.photoPath).toString();
    case ActiveRole: return row.active;
    default: return {};
    }
}

QHash<int, QByteArray> ProductListModel::roleNames() const
{
    return {
        {ProductIdRole, "productId"},
        {NameFrRole, "nameFr"},
        {NameArRole, "nameAr"},
        {BotanicalNameRole, "botanicalName"},
        {CategoryFrRole, "categoryFr"},
        {CategoryArRole, "categoryAr"},
        {IsPlantRole, "isPlant"},
        {VariantCountRole, "variantCount"},
        {MinPriceRole, "minPrice"},
        {PhotoUrlRole, "photoUrl"},
        {ActiveRole, "active"},
    };
}

void ProductListModel::setRows(QList<ProductRow> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

const ProductRow* ProductListModel::rowAt(int index) const
{
    if (index < 0 || index >= m_rows.size())
        return nullptr;
    return &m_rows.at(index);
}

} // namespace nursera
