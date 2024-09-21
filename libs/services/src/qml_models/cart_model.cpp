#include "qml_models/cart_model.h"

#include <QLocale>

namespace nursera {

CartModel::CartModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int CartModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_lines.size());
}

QVariant CartModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return {};

    const SaleLine& line = m_lines.at(index.row());
    switch (role) {
    case VariantIdRole: return line.variantId;
    case LabelRole: return line.label;
    case QtyRole: return line.qty;
    case UnitPriceRole: return line.unitPrice.toDisplayString(QLocale());
    case UnitPriceRawRole: {
        const qint64 m = line.unitPrice.millimes();
        return QStringLiteral("%1,%2").arg(m / 1000).arg(m % 1000, 3, 10,
                                                         QLatin1Char('0'));
    }
    case LineTotalRole: return line.lineTotal().toDisplayString(QLocale());
    case ManualPriceRole: return line.manualPrice;
    default: return {};
    }
}

QHash<int, QByteArray> CartModel::roleNames() const
{
    return {
        {VariantIdRole, "variantId"},
        {LabelRole, "label"},
        {QtyRole, "qty"},
        {UnitPriceRole, "unitPrice"},
        {UnitPriceRawRole, "unitPriceRaw"},
        {LineTotalRole, "lineTotal"},
        {ManualPriceRole, "manualPrice"},
    };
}

void CartModel::addLine(const SaleLine& line)
{
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines[i].variantId == line.variantId) {
            m_lines[i].qty += line.qty;
            const QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {QtyRole, LineTotalRole});
            return;
        }
    }
    beginInsertRows({}, static_cast<int>(m_lines.size()),
                    static_cast<int>(m_lines.size()));
    m_lines.append(line);
    endInsertRows();
}

void CartModel::setQty(int row, int qty)
{
    if (row < 0 || row >= m_lines.size() || qty <= 0)
        return;
    m_lines[row].qty = qty;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {QtyRole, LineTotalRole});
}

void CartModel::setUnitPrice(int row, Money price)
{
    if (row < 0 || row >= m_lines.size() || price.isNegative())
        return;
    m_lines[row].unitPrice = price;
    m_lines[row].manualPrice = true;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx,
                     {UnitPriceRole, UnitPriceRawRole, LineTotalRole, ManualPriceRole});
}

void CartModel::removeAt(int row)
{
    if (row < 0 || row >= m_lines.size())
        return;
    beginRemoveRows({}, row, row);
    m_lines.removeAt(row);
    endRemoveRows();
}

void CartModel::clear()
{
    beginResetModel();
    m_lines.clear();
    endResetModel();
}

void CartModel::repriceForPro(bool pro)
{
    for (int i = 0; i < m_lines.size(); ++i) {
        SaleLine& line = m_lines[i];
        if (line.manualPrice) // un prix négocié prime sur la bascule pro
            continue;
        line.unitPrice = (pro && line.unitPricePro.millimes() > 0)
            ? line.unitPricePro
            : line.unitPriceRegular;
        const QModelIndex idx = index(i);
        emit dataChanged(idx, idx, {UnitPriceRole, UnitPriceRawRole, LineTotalRole});
    }
}

void CartModel::setLines(QList<SaleLine> lines)
{
    beginResetModel();
    m_lines = std::move(lines);
    endResetModel();
}

Money CartModel::subtotal() const
{
    Money total;
    for (const SaleLine& line : m_lines)
        total = total + line.lineTotal();
    return total;
}

int CartModel::itemCount() const
{
    int count = 0;
    for (const SaleLine& line : m_lines)
        count += line.qty;
    return count;
}

} // namespace nursera
