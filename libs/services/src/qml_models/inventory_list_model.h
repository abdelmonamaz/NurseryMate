#pragma once

#include "models/inventory.h"

#include <QAbstractListModel>
#include <QList>

namespace nursera {

// Lignes de l'inventaire en cours (comptage guidé, F03-06).
class InventoryListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        VariantIdRole = Qt::UserRole + 1,
        ProductFrRole,
        ProductArRole,
        PackagingRole,
        SkuRole,
        QtyExpectedRole,
        QtyCountedRole, // -1 = non compté
        IsCountedRole,
        GapRole,
    };

    explicit InventoryListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setLines(QList<InventoryLine> lines);
    // Met à jour une ligne en place (évite le reset du ListView pendant le comptage).
    void updateCounted(int variantId, int qty);

    int countedCount() const;
    int gapCount() const;
    int gapTotal() const;

private:
    QList<InventoryLine> m_lines;
};

} // namespace nursera
