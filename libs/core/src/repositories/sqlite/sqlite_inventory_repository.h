#pragma once

#include "repositories/iinventory_repository.h"
#include "repositories/istock_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteInventoryRepository : public IInventoryRepository
{
public:
    // Dépend de IStockRepository : la validation génère les ajustements
    // par recordMove (transactionnel + idempotent).
    SqliteInventoryRepository(QString connectionName, IStockRepository& stock);

    Result<Inventory> startOrResume(int locationId) override;
    Result<Inventory> resumeAnyDraft() override;
    Result<QList<InventoryLine>> linesOf(int inventoryId) override;
    Result<void> setCounted(int inventoryId, int variantId, int qty) override;
    Result<int> validate(int inventoryId) override;
    Result<void> cancel(int inventoryId) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }
    Result<Inventory> loadByLocation(int locationId);

    QString m_connectionName;
    IStockRepository& m_stock;
};

} // namespace nursera
