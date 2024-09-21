#pragma once

#include "models/stock.h"

#include <QAbstractListModel>
#include <QList>

namespace nursera {

// Modèle de la vue d'ensemble du stock (une ligne par variante).
class StockListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        VariantIdRole = Qt::UserRole + 1,
        ProductFrRole,
        ProductArRole,
        PackagingRole,
        SkuRole,
        QtyRole,
        AlertThresholdRole,
        IsLowRole,      // qty <= seuil (F03-05)
        IsNegativeRole, // stock négatif signalé, jamais bloquant (RG-03.b)
    };

    explicit StockListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QList<StockOverviewRow> rows);

private:
    QList<StockOverviewRow> m_rows;
};

} // namespace nursera
