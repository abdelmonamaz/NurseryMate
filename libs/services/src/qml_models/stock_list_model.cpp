#include "qml_models/stock_list_model.h"

#include <utility>

namespace nursera {

StockListModel::StockListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int StockListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant StockListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const StockOverviewRow& row = m_rows.at(index.row());
    switch (role) {
    case VariantIdRole: return row.variantId;
    case ProductFrRole: return row.productFr;
    case ProductArRole: return row.productAr;
    case PackagingRole: return row.packaging;
    case SkuRole: return row.sku;
    case QtyRole: return row.qty;
    case AlertThresholdRole: return row.alertThreshold;
    case IsLowRole:
        return row.alertThreshold >= 0 && row.qty <= row.alertThreshold;
    case IsNegativeRole: return row.qty < 0;
    default: return {};
    }
}

QHash<int, QByteArray> StockListModel::roleNames() const
{
    return {
        {VariantIdRole, "variantId"},
        {ProductFrRole, "productFr"},
        {ProductArRole, "productAr"},
        {PackagingRole, "packaging"},
        {SkuRole, "sku"},
        {QtyRole, "qty"},
        {AlertThresholdRole, "alertThreshold"},
        {IsLowRole, "isLow"},
        {IsNegativeRole, "isNegative"},
    };
}

void StockListModel::setRows(QList<StockOverviewRow> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

} // namespace nursera
