#pragma once

#include "repositories/isale_repository.h"
#include "repositories/istock_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteSaleRepository : public ISaleRepository
{
public:
    // Dépend de IStockRepository : les sorties de stock de la vente sont
    // enregistrées via recordMove(ownTransaction=false) dans la
    // transaction de la vente.
    SqliteSaleRepository(QString connectionName, IStockRepository& stock);

    Result<Sale> record(const SaleDraft& draft) override;
    Result<SaleDetails> details(int saleId) override;
    Result<void> cancel(int saleId, const QString& reason, int userId) override;
    Result<CreditNote> createCreditNote(const CreditNoteDraft& draft) override;
    Result<QList<RefundableLine>> refundableLines(int saleId) override;
    Result<CreditNoteDetails> creditNoteDetails(int saleId) override;
    Result<QList<SaleJournalRow>> todayJournal() override;
    Result<DayTotals> todayTotals() override;
    Result<Money> recordCashClosure(Money countedCash, int userId) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }
    // Numérotation sans trou T-AAAA-NNNNN (RG-04.b) — dans la transaction.
    Result<QString> nextTicketNumber();

    QString m_connectionName;
    IStockRepository& m_stock;
};

} // namespace nursera
