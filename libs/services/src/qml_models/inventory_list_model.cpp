#include "qml_models/inventory_list_model.h"

#include <utility>

namespace nursera {

InventoryListModel::InventoryListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int InventoryListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_lines.size());
}

QVariant InventoryListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return {};

    const InventoryLine& line = m_lines.at(index.row());
    switch (role) {
    case VariantIdRole: return line.variantId;
    case ProductFrRole: return line.productFr;
    case ProductArRole: return line.productAr;
    case PackagingRole: return line.packaging;
    case SkuRole: return line.sku;
    case QtyExpectedRole: return line.qtyExpected;
    case QtyCountedRole: return line.qtyCounted;
    case IsCountedRole: return line.isCounted();
    case GapRole: return line.gap();
    default: return {};
    }
}

QHash<int, QByteArray> InventoryListModel::roleNames() const
{
    return {
        {VariantIdRole, "variantId"},
        {ProductFrRole, "productFr"},
        {ProductArRole, "productAr"},
        {PackagingRole, "packaging"},
        {SkuRole, "sku"},
        {QtyExpectedRole, "qtyExpected"},
        {QtyCountedRole, "qtyCounted"},
        {IsCountedRole, "isCounted"},
        {GapRole, "gap"},
    };
}

void InventoryListModel::setLines(QList<InventoryLine> lines)
{
    beginResetModel();
    m_lines = std::move(lines);
    endResetModel();
}

void InventoryListModel::updateCounted(int variantId, int qty)
{
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines[i].variantId != variantId)
            continue;
        m_lines[i].qtyCounted = qty;
        const QModelIndex idx = index(i);
        emit dataChanged(idx, idx, {QtyCountedRole, IsCountedRole, GapRole});
        return;
    }
}

int InventoryListModel::countedCount() const
{
    int counted = 0;
    for (const InventoryLine& line : m_lines)
        if (line.isCounted())
            ++counted;
    return counted;
}

int InventoryListModel::gapCount() const
{
    int gaps = 0;
    for (const InventoryLine& line : m_lines)
        if (line.isCounted() && line.gap() != 0)
            ++gaps;
    return gaps;
}

int InventoryListModel::gapTotal() const
{
    int total = 0;
    for (const InventoryLine& line : m_lines)
        if (line.isCounted())
            total += line.gap();
    return total;
}

} // namespace nursera
