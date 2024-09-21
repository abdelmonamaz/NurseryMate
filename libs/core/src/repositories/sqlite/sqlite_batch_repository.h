#pragma once

#include "repositories/ibatch_repository.h"
#include "repositories/istock_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteBatchRepository : public IBatchRepository
{
public:
    // Dépend de IStockRepository : le passage en vendable crée une entrée
    // de stock via recordMove(ownTransaction=false).
    SqliteBatchRepository(QString connectionName, IStockRepository& stock);

    Result<Batch> create(const BatchDraft& draft) override;
    Result<QList<BatchRow>> list(bool includeClosed = false) override;
    Result<void> recordLoss(int batchId, int qty, const QString& lossReason,
                            const QString& note, int userId) override;
    Result<void> recordSellable(int batchId, int qty, int variantId,
                                int locationId, int userId) override;
    Result<QList<BatchEventRow>> events(int batchId) override;
    Result<void> recordTreatment(int batchId, const QString& kind,
                                 const QString& productUsed,
                                 const QString& dose, const QString& note,
                                 int userId) override;
    Result<QList<BatchTreatmentRow>> treatments(int batchId) override;
    Result<void> cancelLastEvent(int batchId, int userId) override;
    Result<bool> isReferenced(int batchId) override;
    Result<void> remove(int batchId) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }
    // Décrémente le restant après contrôle de disponibilité (RG-08.b).
    Result<void> consumeRemaining(QSqlDatabase& database, int batchId, int qty);

    QString m_connectionName;
    IStockRepository& m_stock;
};

} // namespace nursera
