#pragma once

#include "repositories/istock_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteStockRepository : public IStockRepository
{
public:
    explicit SqliteStockRepository(QString connectionName);

    Result<void> recordMove(StockMove move, bool ownTransaction = true) override;
    Result<QList<StockLevel>> levelsOf(int variantId) override;
    Result<QList<StockOverviewRow>> overview(const QString& term,
                                             int locationId = 0) override;
    Result<QList<VariantPick>> searchVariants(const QString& term,
                                              int limit = 10) override;
    Result<QList<StockMoveRow>> history(int variantId = 0,
                                        int limit = 100) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }
    Result<void> applyDelta(int variantId, int locationId, int delta);

    QString m_connectionName;
};

} // namespace nursera
